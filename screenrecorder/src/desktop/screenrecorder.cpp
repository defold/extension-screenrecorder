#if defined(DM_PLATFORM_OSX) || defined(DM_PLATFORM_LINUX) || defined(DM_PLATFORM_WINDOWS) || defined(DM_PLATFORM_HTML5)

#include <string>

#include "screenrecorder.h"
#include "utils.h"

// Extended OpenGL API functions.
#if defined(DM_PLATFORM_LINUX) || defined(DM_PLATFORM_WINDOWS)
	#if defined(DM_PLATFORM_WINDOWS)
		static PFNGLACTIVETEXTUREPROC glActiveTexture = NULL;
	#endif
	static PFNGLISPROGRAMPROC glIsProgram = NULL;
	static PFNGLDELETEPROGRAMPROC glDeleteProgram = NULL;
	static PFNGLISBUFFERPROC glIsBuffer = NULL;
	static PFNGLDELETEBUFFERSPROC glDeleteBuffers = NULL;
	static PFNGLCREATESHADERPROC glCreateShader = NULL;
	static PFNGLSHADERSOURCEPROC glShaderSource = NULL;
	static PFNGLCOMPILESHADERPROC glCompileShader = NULL;
	static PFNGLGETSHADERIVPROC glGetShaderiv = NULL;
	static PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog = NULL;
	static PFNGLCREATEPROGRAMPROC glCreateProgram = NULL;
	static PFNGLATTACHSHADERPROC glAttachShader = NULL;
	static PFNGLLINKPROGRAMPROC glLinkProgram = NULL;
	static PFNGLGETPROGRAMIVPROC glGetProgramiv = NULL;
	static PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog = NULL;
	static PFNGLDELETESHADERPROC glDeleteShader = NULL;
	static PFNGLGENBUFFERSPROC glGenBuffers = NULL;
	static PFNGLBINDBUFFERPROC glBindBuffer = NULL;
	static PFNGLBUFFERDATAPROC glBufferData = NULL;
	static PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = NULL;
	static PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation = NULL;
	static PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer = NULL;
	static PFNGLISFRAMEBUFFERPROC glIsFramebuffer = NULL;
	static PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers = NULL;
	static PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers = NULL;
	static PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer = NULL;
	static PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D = NULL;
	static PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus = NULL;
	static PFNGLDRAWBUFFERSPROC glDrawBuffers = NULL;
	static PFNGLUSEPROGRAMPROC glUseProgram = NULL;
	static PFNGLUNIFORM1IPROC glUniform1i = NULL;
	static PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray = NULL;
	static PFNGLUNIFORM2FPROC glUniform2f = NULL;
	static PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray = NULL;
	static PFNGLGETBUFFERPARAMETERIVPROC glGetBufferParameteriv = NULL;
	static PFNGLUNMAPBUFFERPROC glUnmapBuffer = NULL;
	static PFNGLMAPBUFFERPROC glMapBuffer = NULL;
#endif

// Adapt to OpenGL ES for HTML5 platform.
#ifdef DM_PLATFORM_HTML5
	#define SHADER_HEADER "#version 100\n"\
	"precision mediump float;\n"
#else
	#define SHADER_HEADER "#version 110\n"
#endif

// Captured gameplay frames are rendered onto a quad model to perform scaling and YUV color space conversion.
static const char *vertex_shader_source = SHADER_HEADER
	"attribute vec2 position;"
	"attribute vec2 texcoord;"
	"varying vec2 var_texcoord;"
	"void main() {"
		"var_texcoord = texcoord;"
		"gl_Position = vec4(position.x, position.y, 0.0, 1.0);"
	"}";

// Convert RGB source frame to YUV frame, required by the encoder.
static const char *fragment_shader_source = SHADER_HEADER
	"varying vec2 var_texcoord;"
	"uniform sampler2D tex0;"
	"uniform vec2 scale;"
	"uniform vec2 resolution;\n"

	"#define Y_COMPONENT 0\n"
	"#define U_COMPONENT 1\n"
	"#define V_COMPONENT 2\n"

	"float width = resolution.x;"
	"float height = resolution.y;"

	// For scaling. Return black color if source pixel is outside of the image area.
	"vec4 get_pixel(vec2 source) {"
		"source = (source - 0.5) / scale + 0.5;"
		"if (source.x >= 0.0 && source.y >= 0.0 && source.x <= 1.0 && source.y <= 1.0) return texture2D(tex0, source);"
		"return vec4(0.0, 0.0, 0.0, 1.0);"
	"}"

	"float rgba_to_yuv(int component, float offset) {"
		"if (component == Y_COMPONENT) {"
			"vec4 rgba = get_pixel(vec2(mod(offset, width) / width, (1.0 - floor(offset / width) / height)));"			
			"return 0.299 * rgba.r + 0.587 * rgba.g + 0.114 * rgba.b;"
		"} else if (component == U_COMPONENT || component == V_COMPONENT) {"
			"vec4 rgba = get_pixel(vec2((mod(offset, width) - 1.0) / width, (1.0 - (floor(offset / width) - 3.0) / height * 2.0)));"
			"if (component == U_COMPONENT) {"
				"return -0.169 * rgba.r - 0.331 * rgba.g + 0.5 * rgba.b + 0.5;"
			"} else {"
				"return 0.5 * rgba.r - 0.419 * rgba.g - 0.081 * rgba.b + 0.5;"
			"}"
		"}"
		"return 0.0;"
	"}"

	"void main() {"
		"if (var_texcoord.y < 0.3333333333) {"
			// Y component.
			"float offset = 3.0 * width * ((var_texcoord.x - 0.5) + var_texcoord.y * height);"
			"gl_FragColor = vec4("
				"rgba_to_yuv(Y_COMPONENT, offset),"
				"rgba_to_yuv(Y_COMPONENT, ++offset),"
				"rgba_to_yuv(Y_COMPONENT, ++offset),"
				"1.0"
			");"
		"} else if (var_texcoord.y < 0.4166666666) {"
			// U component.
			"float offset = 6.0 * width * (var_texcoord.x + (var_texcoord.y - 0.3333333333) * height);"
			"gl_FragColor = vec4("
				"rgba_to_yuv(U_COMPONENT, offset),"
				"rgba_to_yuv(U_COMPONENT, offset += 2.0),"
				"rgba_to_yuv(U_COMPONENT, offset += 2.0),"
				"1.0"
			");"
		"} else if (var_texcoord.y < 0.5) {"
			// V component.
			"float offset = 6.0 * width * (var_texcoord.x + (var_texcoord.y - 0.4166666666) * height);"
			"gl_FragColor = vec4("
				"rgba_to_yuv(V_COMPONENT, offset),"
				"rgba_to_yuv(V_COMPONENT, offset += 2.0),"
				"rgba_to_yuv(V_COMPONENT, offset += 2.0),"
				"1.0"
			");"
		"} else {"
			// Unused. YUV frame takes twice less space than RGB frame.
			"gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);"
		"}"
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

// Use Pixel Buffer Objects for better performance. Not available on HTML5.
static const int PBO_COUNT = 3;

static void clear_gl_errors() {
	while (glGetError() != GL_NO_ERROR) {}
}

static int encoding_thread_proc(void *user_data) {
	thread_set_high_priority();
	ScreenRecorder *sr = static_cast<ScreenRecorder *>(user_data);
	while (true) {
		thread_signal_wait(&sr->encoding_signal, THREAD_SIGNAL_WAIT_INFINITE);
		if (!sr->should_encoding_thread_exit) {
			sr->is_enconding_thread_available = false;
			sr->encode_frame(false);
			thread_signal_raise(&sr->encoding_done_signal);
			sr->is_enconding_thread_available = true;
		} else {
			break;
		}
	}
	return 0;
}

ScreenRecorder::ScreenRecorder() :
	// Use pixels buffer instead of PBO on HTML5.
	#ifdef DM_PLATFORM_HTML5
		pixels(NULL),
	#endif
	scaled_texture(0),
	shader_program(0),
	vertex_buffer(0),
	position_attrib(0),
	texcoord_attrib(0),
	tex_uniform(0),
	scale_uniform(0),
	resolution_uniform(0),
	fbo(0),
	pbo_index(0),
	is_pbo_full(false),
	frame_count(0),
	circular_buffer(NULL),
	encoding_thread(NULL),
	is_initialized(false),
	should_encoding_thread_exit(false),
	is_enconding_thread_available(true),
	capture_params() {
		// Load OpenGL functions.
		#if defined(DM_PLATFORM_LINUX) || defined(DM_PLATFORM_WINDOWS)
			#if defined(DM_PLATFORM_WINDOWS)
				GET_PROC_ADDRESS(glActiveTexture, "glActiveTexture", PFNGLACTIVETEXTUREPROC)
			#endif
			GET_PROC_ADDRESS(glIsProgram, "glIsProgram", PFNGLISPROGRAMPROC)
			GET_PROC_ADDRESS(glDeleteProgram, "glDeleteProgram", PFNGLDELETEPROGRAMPROC)
			GET_PROC_ADDRESS(glIsBuffer, "glIsBuffer", PFNGLISBUFFERPROC)
			GET_PROC_ADDRESS(glDeleteBuffers, "glDeleteBuffers", PFNGLDELETEBUFFERSPROC)
			GET_PROC_ADDRESS(glCreateShader, "glCreateShader", PFNGLCREATESHADERPROC)
			GET_PROC_ADDRESS(glShaderSource, "glShaderSource", PFNGLSHADERSOURCEPROC)
			GET_PROC_ADDRESS(glCompileShader, "glCompileShader", PFNGLCOMPILESHADERPROC)
			GET_PROC_ADDRESS(glGetShaderiv, "glGetShaderiv", PFNGLGETSHADERIVPROC)
			GET_PROC_ADDRESS(glGetShaderInfoLog, "glGetShaderInfoLog", PFNGLGETSHADERINFOLOGPROC)
			GET_PROC_ADDRESS(glCreateProgram, "glCreateProgram", PFNGLCREATEPROGRAMPROC)
			GET_PROC_ADDRESS(glAttachShader, "glAttachShader", PFNGLATTACHSHADERPROC)
			GET_PROC_ADDRESS(glLinkProgram, "glLinkProgram", PFNGLLINKPROGRAMPROC)
			GET_PROC_ADDRESS(glGetProgramiv, "glGetProgramiv", PFNGLGETPROGRAMIVPROC)
			GET_PROC_ADDRESS(glGetProgramInfoLog, "glGetProgramInfoLog", PFNGLGETPROGRAMINFOLOGPROC)
			GET_PROC_ADDRESS(glDeleteShader, "glDeleteShader", PFNGLDELETESHADERPROC)
			GET_PROC_ADDRESS(glGenBuffers, "glGenBuffers", PFNGLGENBUFFERSPROC)
			GET_PROC_ADDRESS(glBindBuffer, "glBindBuffer", PFNGLBINDBUFFERPROC)
			GET_PROC_ADDRESS(glBufferData, "glBufferData", PFNGLBUFFERDATAPROC)
			GET_PROC_ADDRESS(glGetUniformLocation, "glGetUniformLocation", PFNGLGETUNIFORMLOCATIONPROC)
			GET_PROC_ADDRESS(glGetAttribLocation, "glGetAttribLocation", PFNGLGETATTRIBLOCATIONPROC)
			GET_PROC_ADDRESS(glVertexAttribPointer, "glVertexAttribPointer", PFNGLVERTEXATTRIBPOINTERPROC)
			GET_PROC_ADDRESS(glIsFramebuffer, "glIsFramebuffer", PFNGLISFRAMEBUFFERPROC)
			GET_PROC_ADDRESS(glDeleteFramebuffers, "glDeleteFramebuffers", PFNGLDELETEFRAMEBUFFERSPROC)
			GET_PROC_ADDRESS(glGenFramebuffers, "glGenFramebuffers", PFNGLGENFRAMEBUFFERSPROC)
			GET_PROC_ADDRESS(glBindFramebuffer, "glBindFramebuffer", PFNGLBINDFRAMEBUFFERPROC)
			GET_PROC_ADDRESS(glFramebufferTexture2D, "glFramebufferTexture2D", PFNGLFRAMEBUFFERTEXTURE2DPROC)
			GET_PROC_ADDRESS(glCheckFramebufferStatus, "glCheckFramebufferStatus", PFNGLCHECKFRAMEBUFFERSTATUSPROC)
			GET_PROC_ADDRESS(glDrawBuffers, "glDrawBuffers", PFNGLDRAWBUFFERSPROC)
			GET_PROC_ADDRESS(glUseProgram, "glUseProgram", PFNGLUSEPROGRAMPROC)
			GET_PROC_ADDRESS(glUniform1i, "glUniform1i", PFNGLUNIFORM1IPROC)
			GET_PROC_ADDRESS(glEnableVertexAttribArray, "glEnableVertexAttribArray", PFNGLENABLEVERTEXATTRIBARRAYPROC)
			GET_PROC_ADDRESS(glUniform2f, "glUniform2f", PFNGLUNIFORM2FPROC)
			GET_PROC_ADDRESS(glDisableVertexAttribArray, "glDisableVertexAttribArray", PFNGLDISABLEVERTEXATTRIBARRAYPROC)
			GET_PROC_ADDRESS(glGetBufferParameteriv, "glGetBufferParameteriv", PFNGLGETBUFFERPARAMETERIVPROC)
			GET_PROC_ADDRESS(glUnmapBuffer, "glUnmapBuffer", PFNGLUNMAPBUFFERPROC)
			GET_PROC_ADDRESS(glMapBuffer, "glMapBuffer", PFNGLMAPBUFFERPROC)
		#endif
	}

ScreenRecorder::~ScreenRecorder() {
	is_initialized = false;
	if (glIsProgram(shader_program)) {
		glDeleteProgram(shader_program);
		shader_program = 0;
		GLenum error = glGetError(); if (error) dmLogError("glDeleteProgram: %#04X", error);
	}
	if (glIsBuffer(vertex_buffer)) {
		glDeleteBuffers(1, &vertex_buffer);
		vertex_buffer = 0;
		GLenum error = glGetError(); if (error) dmLogError("glDeleteBuffers: %#04X", error);
	}
	if (encoding_thread != NULL) {
		// Finish encoding thread.
		should_encoding_thread_exit = true;
		thread_signal_raise(&encoding_signal);
		thread_join(encoding_thread);
		thread_destroy(encoding_thread);
		thread_signal_term(&encoding_signal);
	}
}

bool ScreenRecorder::init(char *error_message) {
	if (is_initialized) {
		return true;
	}
	clear_gl_errors();
	GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	GLenum error = glGetError(); if (error) {ERROR_MESSAGE("glCreateShader: %#04X", error); return false;}
	glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
	error = glGetError(); if (error) {ERROR_MESSAGE("glShaderSource: %#04X", error); return false;}
	glCompileShader(vertex_shader);

	GLint status;
	glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &status);
	if (!status) {
		char buffer[512];
		glGetShaderInfoLog(vertex_shader, 512, NULL, buffer);
		ERROR_MESSAGE("Failed to compile vertex shader:\n%s", buffer);
		return false;
	}

	GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
	glCompileShader(fragment_shader);

	glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &status);
	if (!status) {
		char buffer[512];
		glGetShaderInfoLog(fragment_shader, 512, NULL, buffer);
		ERROR_MESSAGE("Failed to compile fragment shader:\n%s", buffer);
		return false;
	}

	shader_program = glCreateProgram();
	error = glGetError(); if (error) {ERROR_MESSAGE("glCreateProgram: %#04X", error); return false;}
	glAttachShader(shader_program, vertex_shader);
	error = glGetError(); if (error) {ERROR_MESSAGE("glAttachShader v: %#04X", error); return false;}
	glAttachShader(shader_program, fragment_shader);
	error = glGetError(); if (error) {ERROR_MESSAGE("glAttachShader f: %#04X", error); return false;}
	glLinkProgram(shader_program);
	error = glGetError(); if (error) {ERROR_MESSAGE("glLinkProgram: %#04X", error); return false;}
	glGetProgramiv(shader_program, GL_LINK_STATUS, &status);
	if (!status) {
		char buffer[512];
		glGetProgramInfoLog(shader_program, 512, NULL, buffer);
		ERROR_MESSAGE("Failed to link shader:\n%s", buffer);
		return false;
	}

	glDeleteShader(vertex_shader);
	error = glGetError(); if (error) {ERROR_MESSAGE("glDeleteShader v: %#04X", error); return false;}
	glDeleteShader(fragment_shader);
	error = glGetError(); if (error) {ERROR_MESSAGE("glDeleteShader f: %#04X", error); return false;}

	glGenBuffers(1, &vertex_buffer);
	error = glGetError(); if (error) {ERROR_MESSAGE("glGenBuffers: %#04X", error); return false;}
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
	error = glGetError(); if (error) {ERROR_MESSAGE("glBindBuffer: %#04X", error); return false;}
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad_model), quad_model, GL_STATIC_DRAW);
	error = glGetError(); if (error) {ERROR_MESSAGE("glBufferData: %#04X", error); return false;}

	tex_uniform = glGetUniformLocation(shader_program, "tex0");
	error = glGetError(); if (error) {ERROR_MESSAGE("glGetUniformLocation position: %#04X", error); return false;}

	scale_uniform = glGetUniformLocation(shader_program, "scale");
	error = glGetError(); if (error) ERROR_MESSAGE("glGetUniformLocation scale: %#04X", error);

	resolution_uniform = glGetUniformLocation(shader_program, "resolution");
	error = glGetError(); if (error) ERROR_MESSAGE("glGetUniformLocation resolution: %#04X", error);

	// position attribute.
	position_attrib = glGetAttribLocation(shader_program, "position");
	error = glGetError(); if (error) {ERROR_MESSAGE("glGetAttribLocation position: %#04X", error); return false;}
	glVertexAttribPointer(position_attrib, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
	error = glGetError(); if (error) {ERROR_MESSAGE("glVertexAttribPointer position: %#04X", error); return false;}

	// texcoord attribute.
	texcoord_attrib = glGetAttribLocation(shader_program, "texcoord");
	error = glGetError(); if (error) {ERROR_MESSAGE("glGetAttribLocation texcoord: %#04X", error); return false;}
	glVertexAttribPointer(texcoord_attrib, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
	error = glGetError(); if (error) {ERROR_MESSAGE("glVertexAttribPointer texcoord: %#04X", error); return false;}

	is_initialized = true;

	#ifndef DM_PLATFORM_HTML5
		thread_signal_init(&encoding_signal);
		thread_signal_init(&encoding_done_signal);
	#endif

	return true;
}

bool ScreenRecorder::start(char *error_message) {
	frame_count = 0;
	int width = *capture_params.width;
	int height = *capture_params.height;

	// Cleaning up here, because in the stop_thread it would crash.
	if (glIsFramebuffer(fbo)) {
		glDeleteFramebuffers(1, &fbo);
		glDeleteBuffers(PBO_COUNT, pbo);
		glDeleteTextures(1, &scaled_texture);
	}

	glGenFramebuffers(1, &fbo);
	GLenum error = glGetError(); if (error) {ERROR_MESSAGE("glGenFramebuffers fbo: %#04X", error); return false;}
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	error = glGetError(); if (error) {ERROR_MESSAGE("glBindFramebuffer fbo: %#04X", error); return false;}
	glGenTextures(1, &scaled_texture);
	error = glGetError(); if (error) {ERROR_MESSAGE("glGenTextures scaled_texture: %#04X", error); return false;}
	glBindTexture(GL_TEXTURE_2D, scaled_texture);
	error = glGetError(); if (error) {ERROR_MESSAGE("glBindTexture scaled_texture: %#04X", error); return false;}
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	error = glGetError(); if (error) {ERROR_MESSAGE("glTexImage2D scaled_texture: %#04X", error); return false;}
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	error = glGetError(); if (error) {ERROR_MESSAGE("glTexParameteri scaled_texture: %#04X", error); return false;}
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	error = glGetError(); if (error) {ERROR_MESSAGE("glTexParameteri scaled_texture: %#04X", error); return false;}
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, scaled_texture, 0);
	error = glGetError(); if (error) {ERROR_MESSAGE("glFramebufferTexture2D fbo scaled_texture: %#04X", error); return false;}
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE) {ERROR_MESSAGE("glCheckFramebufferStatus: %#04X", error); return false;}
	#ifdef DM_PLATFORM_HTML5
		pixels = new uint8_t[3 * 8 * width * height];
	#else
		GLenum draw_buffers[1] = {GL_COLOR_ATTACHMENT0};
		glDrawBuffers(1, draw_buffers);
	#endif
	error = glGetError(); if (error) {ERROR_MESSAGE("glDrawBuffers: %#04X", error); return false;}
	glBindTexture(GL_TEXTURE_2D, 0);
	error = glGetError(); if (error) {ERROR_MESSAGE("glBindTexture 0: %#04X", error); return false;}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	error = glGetError(); if (error) {ERROR_MESSAGE("glBindFramebuffer 0: %#04X", error); return false;}

	#ifndef DM_PLATFORM_HTML5
		pbo_index = 0;
		is_pbo_full = false;
		glGenBuffers(PBO_COUNT, pbo);
		error = glGetError(); if (error) {ERROR_MESSAGE("glGenBuffers pbo: %#04X", error); return false;}
		for (int i = 0; i < PBO_COUNT; ++i) {
			glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo[i]);
			error = glGetError(); if (error) {ERROR_MESSAGE("glBindBuffer pbo[%d]: %#04X", i, error); return false;}
			glBufferData(GL_PIXEL_PACK_BUFFER, width * height * 3, NULL, GL_STREAM_READ);
			error = glGetError(); if (error) {ERROR_MESSAGE("glBufferData %d: %#04X", i, error); return false;}
		}
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
		error = glGetError(); if (error) {ERROR_MESSAGE("glBindBuffer 0: %#04X", error); return false;}
	#endif

	if (!vpx_img_alloc(&image, VPX_IMG_FMT_I420, width, height, 1)) {
		ERROR_MESSAGE("Failed to allocate image.");
		return false;
	}

	vpx_codec_iface_t *codec_interface = vpx_codec_vp8_cx();
	if (vpx_codec_enc_config_default(codec_interface, &encoder_config, 0)) {
		ERROR_MESSAGE("Failed to get default codec config.");
		return false;
	}

	encoder_config.g_w = width;
	encoder_config.g_h = height;
	encoder_config.g_timebase.num = 1;
	encoder_config.g_timebase.den = *capture_params.fps;
	encoder_config.rc_target_bitrate = *capture_params.bitrate;
	#ifdef DM_PLATFORM_HTML5
		encoder_config.g_threads = 0;
	#else
		encoder_config.g_threads = 4;
	#endif
	encoder_config.g_pass = VPX_RC_ONE_PASS;
	encoder_config.rc_end_usage = VPX_VBR;
	encoder_config.rc_resize_allowed = 0;
	encoder_config.rc_min_quantizer = 2;
	encoder_config.rc_max_quantizer = 50;
	encoder_config.rc_buf_initial_sz = 4000;
	encoder_config.rc_buf_optimal_sz = 5000;
	encoder_config.rc_buf_sz = 6000;
	encoder_config.rc_dropframe_thresh = 25;
	encoder_config.kf_mode = VPX_KF_AUTO;
	encoder_config.kf_max_dist = *capture_params.iframe * *capture_params.fps;

	if (vpx_codec_enc_init(&codec, codec_interface, &encoder_config, 0)) {
		ERROR_MESSAGE("Failed to initialize encoder: %s", codec.err_detail);
		return false;
	}

	if (capture_params.duration != NULL) {
		circular_buffer = new CircularBuffer();
		double duration = *capture_params.duration + *capture_params.iframe; // Increase duration by keyframe interval.
		size_t buffer_size = 1.5 * duration * (*capture_params.bitrate / 8); // Allocate enough memory for frames, plus a bit more for bitrate fluctuation.
		if (!circular_buffer->init(buffer_size, duration * *capture_params.fps)) {
			ERROR_MESSAGE("Failed to initialize circular encoder, requested %zu bytes.", buffer_size);
			return false;
		}
	}

	if (!webm_writer.open(capture_params.filename, width, height, *capture_params.fps)) {
		ERROR_MESSAGE("Failed to open %s for writing.", capture_params.filename);
		return false;
	}

	should_encoding_thread_exit = false;
	is_enconding_thread_available = true;
	if (*capture_params.async_encoding) {
		encoding_thread = thread_create(encoding_thread_proc, this, "Encoding thread", THREAD_STACK_SIZE_DEFAULT);
	} else {
		encoding_thread = NULL;
	}

	return true;
}

// Draw the quad model with retrived texture from Defold's render target, capture the output as YUV video frame and
// pass it into the video encoder.
bool ScreenRecorder::capture_frame(char *error_message) {
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	GLenum error = glGetError(); if (error) {ERROR_MESSAGE("glBindFramebuffer fbo: %#04X", error); return false;}

	glViewport(0, 0, *capture_params.width, *capture_params.height);
	error = glGetError(); if (error) {ERROR_MESSAGE("glViewport: %#04X", error); return false;}

	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(shader_program);
	error = glGetError(); if (error) {ERROR_MESSAGE("glUseProgram: %#04X", error); return false;}

	glActiveTexture(GL_TEXTURE0);
	error = glGetError(); if (error) {ERROR_MESSAGE("glActiveTexture: %#04X", error); return false;}
	glBindTexture(GL_TEXTURE_2D, capture_params.texture_id);
	error = glGetError(); if (error) {ERROR_MESSAGE("glBindTexture capture_params.texture_id: %#04X", error); return false;}
	glUniform1i(tex_uniform, 0);
	error = glGetError(); if (error) {ERROR_MESSAGE("glUniform1i: %#04X", error); return false;}

	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
	error = glGetError(); if (error) {ERROR_MESSAGE("glBindBuffer: %#04X", error); return false;}
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad_model), quad_model, GL_STATIC_DRAW);
	error = glGetError(); if (error) {ERROR_MESSAGE("glBufferData: %#04X", error); return false;}

	glVertexAttribPointer(position_attrib, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
	error = glGetError(); if (error) {ERROR_MESSAGE("glVertexAttribPointer position: %#04X", error); return false;}
	glVertexAttribPointer(texcoord_attrib, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
	error = glGetError(); if (error) {ERROR_MESSAGE("glVertexAttribPointer texcoord: %#04X", error); return false;}

	glEnableVertexAttribArray(position_attrib);
	error = glGetError(); if (error) {ERROR_MESSAGE("glEnableVertexAttribArray position_attrib: %#04X", error); return false;}

	glEnableVertexAttribArray(texcoord_attrib);
	error = glGetError(); if (error) {ERROR_MESSAGE("glEnableVertexAttribArray texcoord_attrib: %#04X", error); return false;}

	glUniform2f(scale_uniform, *capture_params.x_scale, *capture_params.y_scale);
	error = glGetError(); if (error) {ERROR_MESSAGE("glUniform2f scale: %#04X", error); return false;}

	glUniform2f(resolution_uniform, *capture_params.width, *capture_params.height);
	error = glGetError(); if (error) {ERROR_MESSAGE("glUniform2f resolution: %#04X", error); return false;}

	glDrawArrays(GL_TRIANGLES, 0, 6);
	error = glGetError(); if (error) {ERROR_MESSAGE("glDrawArrays: %#04X", error); return false;}

	glBindTexture(GL_TEXTURE_2D, 0);
	error = glGetError(); if (error) {ERROR_MESSAGE("glBindTexture 0: %#04X", error); return false;}

	glDisableVertexAttribArray(position_attrib);
	error = glGetError(); if (error) {ERROR_MESSAGE("glDisableVertexAttribArray position_attrib: %#04X", error); return false;}
	glDisableVertexAttribArray(texcoord_attrib);
	error = glGetError(); if (error) {ERROR_MESSAGE("glDisableVertexAttribArray texcoord_attrib: %#04X", error); return false;}

	glUseProgram(0);
	error = glGetError(); if (error) {ERROR_MESSAGE("glUseProgram 0: %#04X", error); return false;}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	error = glGetError(); if (error) {ERROR_MESSAGE("glBindBuffer 0: %#04X", error); return false;}
	
	#ifdef DM_PLATFORM_HTML5
		int w = *capture_params.width;
		int h = *capture_params.height;
		glReadPixels(0, 0, w, h / 2, GL_RGB, GL_UNSIGNED_BYTE, pixels);
		error = glGetError(); if (error) {ERROR_MESSAGE("glReadPixels: %#04X", error); return false;}
		image.planes[0] = pixels; // Y frame.
		image.planes[1] = image.planes[0] + w * h; // U frame.
		image.planes[2] = image.planes[1] + w * h / 4; // V frame.
		encode_frame(false);
	#else
		glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo[pbo_index]);
		error = glGetError(); if (error) {ERROR_MESSAGE("glBindBuffer GL_PIXEL_PACK_BUFFER: %#04X", error); return false;}

		GLint is_mapped = GL_FALSE;
		glGetBufferParameteriv(GL_PIXEL_PACK_BUFFER, GL_BUFFER_MAPPED, &is_mapped);
		if (is_mapped) {
			glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
			error = glGetError(); if (error) {ERROR_MESSAGE("glUnmapBuffer GL_PIXEL_PACK_BUFFER: %#04X", error); return false;}
		}

		glReadPixels(0, 0, *capture_params.width, *capture_params.height / 2, GL_RGB, GL_UNSIGNED_BYTE, 0);
		error = glGetError(); if (error) {ERROR_MESSAGE("glReadPixels: %#04X", error); return false;}

		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
		error = glGetError(); if (error) {ERROR_MESSAGE("glBindBuffer GL_PIXEL_PACK_BUFFER 0: %#04X", error); return false;}
	#endif

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	error = glGetError(); if (error) {ERROR_MESSAGE("glBindFramebuffer 0: %#04X", error); return false;}

	#ifndef DM_PLATFORM_HTML5
		if (is_pbo_full) {
			if (*capture_params.async_encoding && !is_enconding_thread_available) {
				thread_signal_wait(&encoding_done_signal, THREAD_SIGNAL_WAIT_INFINITE);
			}
			glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo[(pbo_index + 1) % PBO_COUNT]);
			int w = *capture_params.width;
			int h = *capture_params.height;
			//GLubyte *pixels = (GLubyte *)glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, 1.5 * w * h, GL_MAP_READ_BIT);
			GLubyte *pixels = (GLubyte *)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
			error = glGetError(); if (error) {ERROR_MESSAGE("glMapBuffer: %#04X", error); return false;}
			if (pixels) {
				image.planes[0] = pixels; // Y frame.
				image.planes[1] = image.planes[0] + w * h; // U frame.
				image.planes[2] = image.planes[1] + w * h / 4; // V frame.
				if (*capture_params.async_encoding) {
					// Signal encoding thread to start encoding.
					thread_signal_raise(&encoding_signal);
				} else {
					encode_frame(false);
				}
			}
			glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
		} else if (pbo_index == PBO_COUNT - 1) {
			is_pbo_full = true;
		}

		pbo_index = (pbo_index + 1) % PBO_COUNT;
	#endif
	return true;
}

bool ScreenRecorder::stop(char *error_message) {
	if (encoding_thread != NULL) {
		dmLogDebug("Finishing encoding thread.");
		// Finish encoding thread.
		should_encoding_thread_exit = true;
		thread_signal_raise(&encoding_signal);
		thread_join(encoding_thread);
		thread_destroy(encoding_thread);
		dmLogDebug("Finished encoding thread.");
		encoding_thread = NULL;
	}
	// Flush encoder.
	while (encode_frame(true)) {
	}
	vpx_img_free(&image);
	if (vpx_codec_destroy(&codec)) {
		ERROR_MESSAGE("Failed to destroy codec.");
		return false;
	}
	#ifdef DM_PLATFORM_HTML5
		delete []pixels;
	#endif
	if (circular_buffer != NULL) {
		int64_t first_timestamp = 0;
		bool got_keyframe = false;
		uint8_t *data = NULL;
		size_t size = 0;
		int64_t timestamp = 0;
		bool is_keyframe = false;
		uint32_t frame_index = 0;
		while (circular_buffer->get_frame(&data, &size, &timestamp, &is_keyframe, &frame_index)) {
			if (!got_keyframe && is_keyframe) {
				got_keyframe = true;
				first_timestamp = timestamp; // Timestamps must start from 0.
			}
			// Must wait for a keyframe first.
			if (got_keyframe) {
				if (!webm_writer.write_frame(data, size, timestamp - first_timestamp, is_keyframe)) {
					ERROR_MESSAGE("Failed to write compressed frame %d.", frame_index);
					return false;
				}
			}
		}
		delete circular_buffer;
		circular_buffer = NULL;
	}
	webm_writer.close();
	return true;
}

bool ScreenRecorder::encode_frame(bool is_flush) {
	bool has_packets = false;
	vpx_codec_iter_t iter = NULL;
	const vpx_codec_cx_pkt_t *pkt = NULL;
	const vpx_codec_err_t res = vpx_codec_encode(&codec, is_flush ? NULL : &image, is_flush ? -1 : frame_count++, 1, 0, VPX_DL_REALTIME);
	if (res != VPX_CODEC_OK) {
		dmLogError("Failed to encode frame.");
		return false;
	}
	while ((pkt = vpx_codec_get_cx_data(&codec, &iter)) != NULL) {
		has_packets = true;
		if (pkt->kind == VPX_CODEC_CX_FRAME_PKT) {
			if (circular_buffer != NULL) {
				if (!circular_buffer->add_frame(static_cast<uint8_t *>(pkt->data.frame.buf), pkt->data.frame.sz, pkt->data.frame.pts, pkt->data.frame.flags & VPX_FRAME_IS_KEY)) {
					dmLogError("Failed to add compressed frame %d to the circular encoder.", frame_count);
				}
			} else if (!webm_writer.write_frame(static_cast<uint8_t *>(pkt->data.frame.buf), pkt->data.frame.sz, pkt->data.frame.pts, pkt->data.frame.flags & VPX_FRAME_IS_KEY)) {
				dmLogError("Failed to write compressed frame %d.", frame_count);
			}
		}
	}
	return has_packets;
}

#endif