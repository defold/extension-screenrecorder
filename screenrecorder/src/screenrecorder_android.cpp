#if defined(DM_PLATFORM_ANDROID)

// On Android a so called Surface Input video encoding is used. Main part is done in Java and JNI is used to glue C++ and Java worlds.
// Surface Input accepts a separate OpenGL ES context, the extension draws a quad model with video frame texture to this context and it's recorderd
// to a video file by Android OS.

#define EGL_EGLEXT_PROTOTYPES 1
#define GL_GLEXT_PROTOTYPES 1

#include <android/native_window_jni.h>
#include <dmsdk/sdk.h>
#include <dmsdk/dlib/log.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "screenrecorder_private.h"
#include "android/java_lua.h"

#define EGL_RECORDABLE_ANDROID 0x3142

static bool is_recording = false;
static bool is_ready = false;
static uint32_t width = 0;
static uint32_t height = 0;
static double x_scale = 0;
static double y_scale = 0;
static EGLConfig egl_config;
static EGLDisplay egl_display = EGL_NO_DISPLAY;
static EGLContext egl_context = EGL_NO_CONTEXT;
static EGLSurface egl_surface = EGL_NO_SURFACE;
static EGLContext defold_egl_context = EGL_NO_CONTEXT;
static EGLSurface defold_egl_surface = EGL_NO_SURFACE;

static ANativeWindow *encoder_surface = NULL;

static uint32_t frame_index = 0;
static GLuint shader_program = 0;
static GLuint vertex_buffer = 0;
static GLint position_attrib = 0;
static GLint texcoord_attrib = 0;
static GLint defold_texture_id = 0;

static const char *vertex_shader_source = "#version 100\n"
	"attribute mediump vec2 position;"
	"attribute mediump vec2 texcoord;"
	"uniform mediump vec2 scale;"
	"varying mediump vec2 var_texcoord;"
	"void main() {"
		"var_texcoord = texcoord;"
		"gl_Position = vec4(position.x * scale.x, position.y * scale.y, 0.0, 1.0);"
	"}";


static const char *fragment_shader_source = "#version 100\n"
	"varying mediump vec2 var_texcoord;"
	"uniform mediump sampler2D tex;"
	"void main() {"
		"gl_FragColor = texture2D(tex, var_texcoord.xy);"
	"}";

// Quad model.
static float quad_model[] = {
	// position		// texture coords
	-1.0f, -1.0f,	0.0f, 0.0f, // left bottom
	 1.0f, -1.0f,	1.0f, 0.0f, // right bottom
	-1.0f,  1.0f,	0.0f, 1.0f, // left top
	-1.0f,  1.0f,	0.0f, 1.0f, // left top
	 1.0f, -1.0f,	1.0f, 0.0f, // right bottom
	 1.0f,  1.0f,	1.0f, 1.0f, // right top
};

// Cache references.
static jobject java_surface_object = NULL;
static jobject screen_recorder_object = NULL;
static jmethodID screen_recorder_finalize = NULL;
static jmethodID screen_recorder_update = NULL;
static jmethodID screen_recorder_get_encoder_surface = NULL;
static jmethodID screen_recorder_init = NULL;
static jmethodID screen_recorder_start = NULL;
static jmethodID screen_recorder_stop = NULL;
static jmethodID screen_recorder_mux_audio_video = NULL;

static bool get_encoder_surface() {
	ThreadAttacher attacher;
	JNIEnv *env = attacher.env;
	java_surface_object = (jobject)env->NewGlobalRef(env->CallObjectMethod(screen_recorder_object, screen_recorder_get_encoder_surface));
	if (java_surface_object == NULL) {
		dmLogError("Failed to get native java surface.");
		return false;
	}
	encoder_surface = ANativeWindow_fromSurface(env, java_surface_object);
	ANativeWindow_acquire(encoder_surface);
	if (encoder_surface == NULL) {
		dmLogError("Failed to get native encoder surface.");
		return false;
	}
	return true;
}

// When screenrecorder.start() is called, the Java part calls this function to indicate a succesful start.
extern "C" JNIEXPORT void JNICALL Java_extension_screenrecorder_ScreenRecorder_start_1native(JNIEnv *env, jobject instance) {
	is_recording = true;
	frame_index = 0;
}

// When screenrecorder.stop() is called, the Java part calls this function to perform a clean up.
extern "C" JNIEXPORT void JNICALL Java_extension_screenrecorder_ScreenRecorder_stop_1native(JNIEnv *env, jobject instance) {
	is_recording = false;
	is_ready = false;
	if (egl_surface != EGL_NO_SURFACE) {
		eglDestroySurface(egl_display, egl_surface);
		egl_surface = EGL_NO_SURFACE;
	}
	if (encoder_surface != NULL) {
		ANativeWindow_release(encoder_surface);
		encoder_surface = NULL;
	}
	if (java_surface_object != NULL) {
		ThreadAttacher attacher;
		attacher.env->DeleteGlobalRef(java_surface_object);
		java_surface_object = NULL;
	}
	if (glIsProgram(shader_program)) {
		glDeleteProgram(shader_program);
		shader_program = 0;
		GLenum error = glGetError(); if (error) dmLogError("glDeleteProgram: %d", error);
	}
	if (glIsBuffer(vertex_buffer)) {
		glDeleteBuffers(1, &vertex_buffer);
		GLenum error = glGetError(); if (error) dmLogError("glDeleteBuffers: %d", error);
	}
}

static bool init_gl() {
	if (!eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context)) {
		dmLogError("EGL: Failed to switch to own surface: %d." , eglGetError());
		return false;
	}

	glViewport(0, 0, width, height);
	GLenum error = glGetError(); if (error) {dmLogError("glViewport: %d", error); return false;}

	GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
	glCompileShader(vertex_shader);

	GLint status;
	glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &status);
	if (!status) {
		dmLogError("Failed to compile vertex shader");
		char buffer[512];
		glGetShaderInfoLog(vertex_shader, 512, NULL, buffer);
		dmLogError(buffer);
		return false;
	}

	GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
	glCompileShader(fragment_shader);

	glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &status);
	if (!status) {
		dmLogError("Failed to compile fragment shader");
		char buffer[512];
		glGetShaderInfoLog(fragment_shader, 512, NULL, buffer);
		dmLogError(buffer);
		return false;
	}

	shader_program = glCreateProgram();
	error = glGetError(); if (error) {dmLogError("glCreateProgram: %d", error); return false;}
	glAttachShader(shader_program, vertex_shader);
	error = glGetError(); if (error) {dmLogError("glAttachShader v: %d", error); return false;}
	glAttachShader(shader_program, fragment_shader);
	error = glGetError(); if (error) {dmLogError("glAttachShader f: %d", error); return false;}
	glLinkProgram(shader_program);
	error = glGetError(); if (error) {dmLogError("glLinkProgram: %d", error); return false;}
	glGetProgramiv(shader_program, GL_LINK_STATUS, &status);
	if (!status) {
		dmLogError("Failed to link shader");
		char buffer[512];
		glGetProgramInfoLog(shader_program, 512, NULL, buffer);
		dmLogError(buffer);
		return false;
	}

	glDeleteShader(vertex_shader);
	error = glGetError(); if (error) {dmLogError("glDeleteShader v: %d", error); return false;}
	glDeleteShader(fragment_shader);
	error = glGetError(); if (error) {dmLogError("glDeleteShader f: %d", error); return false;}

	glGenBuffers(1, &vertex_buffer);
	error = glGetError(); if (error) {dmLogError("glGenBuffers: %d", error); return false;}
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
	error = glGetError(); if (error) {dmLogError("glBindBuffer: %d", error); return false;}
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad_model), quad_model, GL_STATIC_DRAW);
	error = glGetError(); if (error) {dmLogError("glBufferData: %d", error); return false;}

	// position attribute.
	position_attrib = glGetAttribLocation(shader_program, "position");
	error = glGetError(); if (error) {dmLogError("glGetAttribLocation position: %d", error); return false;}
	glVertexAttribPointer(position_attrib, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
	error = glGetError(); if (error) {dmLogError("glVertexAttribPointer position: %d", error); return false;}

	// texcoord attribute.
	texcoord_attrib = glGetAttribLocation(shader_program, "texcoord");
	error = glGetError(); if (error) {dmLogError("glGetAttribLocation texcoord: %d", error); return false;}
	glVertexAttribPointer(texcoord_attrib, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
	error = glGetError(); if (error) {dmLogError("glVertexAttribPointer texcoord: %d", error); return false;}

	defold_egl_surface = dmGraphics::GetNativeAndroidEGLSurface();
	if (!eglMakeCurrent(egl_display, defold_egl_surface, defold_egl_surface, defold_egl_context)) {
		dmLogError("EGL: Failed to switch to defold surface: %d." , eglGetError());
		return false;
	}
	return true;
}

// When screenrecorder.init() is called, the Java part calls this function to finish initialization.
extern "C" JNIEXPORT jboolean JNICALL Java_extension_screenrecorder_ScreenRecorder_init_1native(JNIEnv *env, jobject instance, jint _width, jint _height, jlong render_target, jdouble _x_scale, jdouble _y_scale) {
	width = _width;
	height = _height;
	x_scale = _x_scale;
	y_scale = _y_scale;

	dmGraphics::HTexture color_texture = dmGraphics::GetRenderTargetAttachment((dmGraphics::HRenderTarget)render_target, dmGraphics::ATTACHMENT_COLOR);
	if (color_texture == NULL) {
		dmLogError("Could not get the color attachment from render target.");
		return false;
	}
	GLuint *color_gl_handle = NULL;
	dmGraphics::HandleResult result = dmGraphics::GetTextureHandle(color_texture, (void **)&color_gl_handle);
	if (result != dmGraphics::HANDLE_RESULT_OK) {
		dmLogError("Could not get texture handle from render target.");
		return false;
	}
	defold_texture_id = *color_gl_handle;

	if (!get_encoder_surface()) {
		return false;
	}

	EGLint const surface_attrib_list[] = {
		EGL_NONE
	};

	egl_display = eglGetCurrentDisplay();
	if (egl_display == EGL_NO_DISPLAY) {
		dmLogError("EGL: Failed to get default display: %d.", eglGetError());
		return false;
	}
	defold_egl_context = eglGetCurrentContext();
	if (defold_egl_context == EGL_NO_CONTEXT) {
		dmLogError("EGL: Failed to get defold egl context: %d.", eglGetError());
		return false;
	}
	defold_egl_surface = dmGraphics::GetNativeAndroidEGLSurface();
	if (defold_egl_surface == EGL_NO_SURFACE) {
		dmLogError("EGL: Failed to get defold egl surface: %d." , eglGetError());
		return false;
	}

	EGLint const config_attrib_list[] = {
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RECORDABLE_ANDROID, 1,
		EGL_NONE
	};
	EGLint num_configs;
	if (!eglChooseConfig(egl_display, config_attrib_list, &egl_config, 1, &num_configs)) {
		dmLogError("EGL: Failed to choose config: %d." , eglGetError());
		return false;
	}

	EGLint const context_attrib_list[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

	if (egl_context != EGL_NO_CONTEXT) {
		if (!eglDestroyContext(egl_display, egl_context)) {
			dmLogError("EGL: Failed to delete context: %d.", eglGetError());
		}
	}

	// Pass defold_egl_context to the newly createad egl_context to share OpenGL instances, render target texture in particular.
	egl_context = eglCreateContext(egl_display, egl_config, defold_egl_context, context_attrib_list);
	if (egl_context == EGL_NO_CONTEXT) {
		dmLogError("EGL: Failed to create context: %d.", eglGetError());
		return false;
	}

	egl_surface = eglCreateWindowSurface(egl_display, egl_config, encoder_surface, surface_attrib_list);
	if (egl_surface == EGL_NO_SURFACE) {
		dmLogError("EGL: Failed to create egl surface: %d." , eglGetError());
		return false;
	}

	if (!init_gl()) {
		return false;
	}

	is_ready = true;
	return true;
}

int ScreenRecorder_init(lua_State *L) {
	int result = 0;
	if (screen_recorder_object != NULL) {
		ThreadAttacher attacher;
		result = attacher.env->CallIntMethod(screen_recorder_object, screen_recorder_init, (jlong)L);
	}
	return result;
}

int ScreenRecorder_start(lua_State *L) {
	int result = 0;
	if (screen_recorder_object != NULL) {
		ThreadAttacher attacher;
		result = attacher.env->CallIntMethod(screen_recorder_object, screen_recorder_start, (jlong)L);
	}
	return result;
}

int ScreenRecorder_stop(lua_State *L) {
	int result = 0;
	if (screen_recorder_object != NULL) {
		ThreadAttacher attacher;
		result = attacher.env->CallIntMethod(screen_recorder_object, screen_recorder_stop, (jlong)L);
	}
	return result;
}

int ScreenRecorder_mux_audio_video(lua_State *L) {
	int result = 0;
	if (screen_recorder_object != NULL) {
		ThreadAttacher attacher;
		result = attacher.env->CallIntMethod(screen_recorder_object, screen_recorder_mux_audio_video, (jlong)L);
	}
	return result;
}

int ScreenRecorder_capture_frame(lua_State *L) {
	if (is_recording) {
		if (!eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context)) {
			dmLogError("EGL: Failed to switch to screenrecorder surface: %d." , eglGetError());
		}
		glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
		GLenum error = glGetError(); if (error) dmLogError("glBindBuffer: %d", error);

		glEnableVertexAttribArray(position_attrib);
		error = glGetError(); if (error) dmLogError("glEnableVertexAttribArray position_attrib: %d", error);
		glEnableVertexAttribArray(texcoord_attrib);
		error = glGetError(); if (error) dmLogError("glEnableVertexAttribArray texcoord_attrib: %d", error);

		glBindTexture(GL_TEXTURE_2D, defold_texture_id);

		glUseProgram(shader_program);
		error = glGetError(); if (error) dmLogError("glUseProgram: %d", error);

		GLint scale_uniform = glGetUniformLocation(shader_program, "scale");
		error = glGetError(); if (error) dmLogError("glGetUniformLocation scale: %d", error);
		glUniform2f(scale_uniform, x_scale, y_scale);
		error = glGetError(); if (error) dmLogError("glUniform2f scale: %d", error);

		glDrawArrays(GL_TRIANGLES, 0, 6);
		error = glGetError(); if (error) dmLogError("glDrawArrays: %d", error);

		glBindTexture(GL_TEXTURE_2D, 0);

		glDisableVertexAttribArray(position_attrib);
		error = glGetError(); if (error) dmLogError("glDisableVertexAttribArray position_attrib: %d", error);
		glDisableVertexAttribArray(texcoord_attrib);
		error = glGetError(); if (error) dmLogError("glDisableVertexAttribArray texcoord_attrib: %d", error);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		error = glGetError(); if (error) dmLogError("glBindBuffer 0: %d", error);
		// Missing symbol.
		//if (!eglPresentationTimeANDROID(egl_display, egl_surface, frame_index * 16666666)) {
		//	dmLogError("EGL: Failed to set presentation time: %d." , eglGetError());
		//}
		if (!eglSwapBuffers(egl_display, egl_surface)) {
			dmLogError("EGL: Failed to swap buffers: %d." , eglGetError());
		}
		++frame_index;
		defold_egl_surface = dmGraphics::GetNativeAndroidEGLSurface();
		if (!eglMakeCurrent(egl_display, defold_egl_surface, defold_egl_surface, defold_egl_context)) {
			dmLogError("EGL: Failed to switch to defold surface: %d." , eglGetError());
		}
	}
	return 0;
}

int ScreenRecorder_is_recording(lua_State *L) {
	lua_pushboolean(L, is_recording);
	return 1;
}
int ScreenRecorder_is_preview_available(lua_State *L) {
	lua_pushboolean(L, false);
	return 1;
}

int ScreenRecorder_show_preview(lua_State *L) {
	return 0;
}

void ScreenRecorder_initialize(lua_State *L) {
	// Mention java_lua.h exports so they don't get optimized away.
	if (L == NULL) {
		Java_extension_screenrecorder_Lua_lua_1registryindex(NULL, NULL, 0);
		Java_extension_screenrecorder_Lua_lua_1gettop(NULL, NULL, 0);
	}
	ThreadAttacher attacher;
	JNIEnv *env = attacher.env;
	ClassLoader class_loader = ClassLoader(env);

	// Invoke ScreenRecorder from the Java extension.
	jclass screen_recorder_class = class_loader.load("extension/" EXTENSION_NAME_STRING "/ScreenRecorder");
	if (screen_recorder_class == NULL) {
		dmLogError("screen_recorder_class is NULL");
	}
	jmethodID screen_recorder_constructor = env->GetMethodID(screen_recorder_class, "<init>", "(Landroid/app/Activity;)V");
	screen_recorder_init = env->GetMethodID(screen_recorder_class, "init", "(J)I");
	screen_recorder_start = env->GetMethodID(screen_recorder_class, "start", "(J)I");
	screen_recorder_stop = env->GetMethodID(screen_recorder_class, "stop", "(J)I");
	screen_recorder_mux_audio_video = env->GetMethodID(screen_recorder_class, "mux_audio_video", "(J)I");
	screen_recorder_finalize = env->GetMethodID(screen_recorder_class, "extension_finalize", "(J)V");
	screen_recorder_update = env->GetMethodID(screen_recorder_class, "update", "(J)V");
	screen_recorder_get_encoder_surface = env->GetMethodID(screen_recorder_class, "get_encoder_surface", "()Landroid/view/Surface;");
	screen_recorder_object = (jobject)env->NewGlobalRef(env->NewObject(screen_recorder_class, screen_recorder_constructor, dmGraphics::GetNativeAndroidActivity()));
	if (screen_recorder_object == NULL) {
		dmLogError("screen_recorder_object is NULL");
	}
}

void ScreenRecorder_update(lua_State *L) {
	if (screen_recorder_object != NULL) {
		ThreadAttacher attacher;
		// Update the Java side so it can invoke any pending listeners.
		attacher.env->CallVoidMethod(screen_recorder_object, screen_recorder_update, (jlong)L);
	}
}

void ScreenRecorder_finalize(lua_State *L) {
	ThreadAttacher attacher;
	if (screen_recorder_object != NULL) {
		attacher.env->CallVoidMethod(screen_recorder_object, screen_recorder_finalize, (jlong)L);
		attacher.env->DeleteGlobalRef(screen_recorder_object);
	}
	if (java_surface_object != NULL) {
		attacher.env->DeleteGlobalRef(java_surface_object);
	}
	screen_recorder_object = NULL;
	java_surface_object = NULL;
}

#endif
