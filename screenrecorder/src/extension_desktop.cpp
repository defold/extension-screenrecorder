#if defined(DM_PLATFORM_OSX) || defined(DM_PLATFORM_LINUX) || defined(DM_PLATFORM_WINDOWS)

#define THREAD_IMPLEMENTATION
#include <thread.h>
#include "extension.h"
#include "desktop/screenrecorder.h"
#include "desktop/utils.h"

static ScreenRecorder *sr = NULL;
static bool is_initialized = false;
static bool is_recording = false;
static int *lua_listener = NULL;
static thread_ptr_t mux_audio_video_thread = NULL;
static thread_ptr_t stop_thread = NULL;

struct MuxAudioVideoUserData {
	char *audio_filename;
	char *video_filename;
	char *filename;
	lua_State *L;
};
MuxAudioVideoUserData mux_audio_video_user_data;

static const char *SCREENRECORDER = "screenrecorder";
static const char *EVENT_INIT = "init";
static const char *EVENT_MUXED = "muxed";
static const char *EVENT_RECORDED = "recorded";

static bool get_render_target_texture_id(void *render_target, int *texture_id) {
	dmGraphics::HTexture color_texture = dmGraphics::GetRenderTargetAttachment((dmGraphics::HRenderTarget)render_target, dmGraphics::ATTACHMENT_COLOR);
	if (color_texture == NULL) {
		dmLogError("Could not get the color attachment from render target.");
		return false;
	}
	uint32_t *color_gl_handle = NULL;
	dmGraphics::HandleResult result = dmGraphics::GetTextureHandle(color_texture, (void **)&color_gl_handle);
	if (result != dmGraphics::HANDLE_RESULT_OK) {
		dmLogError("Could not get texture handle from render target.");
		return false;
	}
	*texture_id = *color_gl_handle;
	return true;
}

static bool check_is_initialized() {
	if (is_initialized) {
		return true;
	} else {
		dmLogInfo("The extension is not initialized.");
		return false;
	}
}

static int extension_enable_debug(lua_State *L) {
	utils::check_arg_count(L, 0);
	utils::enable_debug();
	return 0;
}

static int extension_get_info(lua_State *L) {
	dmLogDebug("get_info()");
	utils::check_arg_count(L, 0);
	lua_createtable(L, 0, 4);

	const char* version = (const char*)glGetString(GL_VERSION);
	lua_pushstring(L, "version");
	lua_pushstring(L, version);
	lua_settable(L, -3);

	lua_pushstring(L, "vendor");
	lua_pushstring(L, (const char*)glGetString(GL_VENDOR));
	lua_settable(L, -3);

	lua_pushstring(L, "renderer");
	lua_pushstring(L, (const char*)glGetString(GL_RENDERER));
	lua_settable(L, -3);

	lua_pushstring(L, "shading_language_version");
	#ifdef GL_SHADING_LANGUAGE_VERSION
		lua_pushstring(L, (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));
	#else
		lua_pushstring(L, "");
	#endif
	lua_settable(L, -3);

	lua_pushstring(L, "extensions");
	lua_pushstring(L, (const char*)glGetString(GL_EXTENSIONS));
	lua_settable(L, -3);

	return 1;
}

static int extension_init(lua_State *L) {
	dmLogDebug("init()");
	utils::check_arg_count(L, 1);
	if (is_initialized) {
		dmLogInfo("init(): The extension is already initialized.");
		return 0;
	}

	void *render_target = NULL;

	utils::get_table(L, 1); // params.
	utils::table_get_string_not_null(L, "filename", &sr->capture_params.filename);
	utils::table_get_integer(L, "width", &sr->capture_params.width, 1280);
	utils::table_get_integer(L, "height", &sr->capture_params.height, 720);
	utils::table_get_integer(L, "bitrate", &sr->capture_params.bitrate, 2 * 1024 * 1024);
	utils::table_get_integer(L, "iframe", &sr->capture_params.iframe, 1);
	utils::table_get_integer(L, "fps", &sr->capture_params.fps, 30);
	utils::table_get_double(L, "duration", &sr->capture_params.duration);
	utils::table_get_double(L, "x_scale", &sr->capture_params.x_scale, 1.0);
	utils::table_get_double(L, "y_scale", &sr->capture_params.y_scale, 1.0);
	utils::table_get_boolean(L, "async_encoding", &sr->capture_params.async_encoding, false);
	utils::table_get_function(L, "listener", &lua_listener, LUA_REFNIL);
	utils::table_get_lightuserdata_not_null(L, "render_target", &render_target);
	lua_pop(L, 1); // params table.

	utils::Event event = {
		.name = SCREENRECORDER,
		.phase = EVENT_INIT,
		.is_error = false
	};

	is_initialized = false;
	is_recording = false;

	int w = *sr->capture_params.width;
	int h = *sr->capture_params.height;

	bool success = get_render_target_texture_id(render_target, &sr->capture_params.texture_id);
	if (!success) {
		event.is_error = true;
		event.error_message = "Failed to retrive render target texture id.";
	} else if (w <= 0 || h <= 0 || (w % 2) != 0 || (h % 2) != 0) {
		event.is_error = true;
		event.error_message = "Invalid width and/or height. Must be positive and divisible by two.";
	} else if (sr->capture_params.duration != NULL && *sr->capture_params.duration < 5.0) {
		event.is_error = true;
		event.error_message = "Too small duration, must be at least 5 seconds.";
	} else if (!sr->init()) {
		event.is_error = true;
		event.error_message = "Failed to initialize OpenGL.";
	} else {
		is_initialized = true;
	}
	
	utils::dispatch_event(L, *lua_listener, &event);
	return 0;
}

int extension_start(lua_State *L) {
	dmLogDebug("start()");
	utils::check_arg_count(L, 0);
	if (!check_is_initialized()) {
		return 0;
	}
	if (!is_recording) {
		bool success = sr->start();
		if (success) {
			is_recording = true;
			dmLogDebug("Recording.");
		} else {
			utils::Event event = {
				.name = SCREENRECORDER,
				.phase = EVENT_INIT,
				.is_error = true,
				.error_message = "Failed to start video recording."
			};
			utils::dispatch_event(L, *lua_listener, &event);
		}
	}
	return 0;
}

static int stop_thread_proc(void *unused) {
	sr->stop();
	dmLogDebug("Recording is done.");
	utils::Event event = {
		.name = SCREENRECORDER,
		.phase = EVENT_RECORDED,
		.is_error = false
	};
	utils::add_task(*lua_listener, &event);
	return 0;
}

static int extension_stop(lua_State *L) {
	dmLogDebug("stop()");
	utils::check_arg_count(L, 0);
	if (is_recording) {
		if (stop_thread != NULL) {
			thread_join(stop_thread);
			thread_destroy(stop_thread);
		}
		stop_thread = thread_create(stop_thread_proc, NULL, "Stop recording thread", THREAD_STACK_SIZE_DEFAULT);
	}
	is_initialized = false;
	is_recording = false;
	return 0;
}

static int mux_audio_video_thread_proc(void *unused) {
	WebmWriter webm_writer;
	webm_writer.mux_audio_video(mux_audio_video_user_data.audio_filename, mux_audio_video_user_data.video_filename, mux_audio_video_user_data.filename);

	utils::Event event = {
		.name = SCREENRECORDER,
		.phase = EVENT_MUXED,
		.is_error = false
	};
	utils::add_task(*lua_listener, &event);
	return 0;
}

static int extension_mux_audio_video(lua_State *L) {
	dmLogDebug("mux_audio_video");
	utils::check_arg_count(L, 1);

	if (mux_audio_video_thread != NULL) {
		thread_join(mux_audio_video_thread);
		thread_destroy(mux_audio_video_thread);
	}

	utils::get_table(L, 1); // params.
	utils::table_get_string_not_null(L, "audio_filename", &mux_audio_video_user_data.audio_filename);
	utils::table_get_string_not_null(L, "video_filename", &mux_audio_video_user_data.video_filename);
	utils::table_get_string_not_null(L, "filename", &mux_audio_video_user_data.filename);
	lua_pop(L, 1); // params table.

	mux_audio_video_thread = thread_create(mux_audio_video_thread_proc, NULL, "Mux audio and video thread", THREAD_STACK_SIZE_DEFAULT);

	return 0;
}

static int extension_capture_frame(lua_State *L) {
	utils::check_arg_count(L, 0);
	if (is_recording) {
		sr->capture_frame();
	}
	return 0;
}

static int extension_is_recording(lua_State *L) {
	dmLogDebug("is_recording");
	utils::check_arg_count(L, 0);
	lua_pushboolean(L, is_recording);
	return 1;
}

static int extension_is_preview_available(lua_State *L) {
	dmLogDebug("is_preview_available");
	utils::check_arg_count(L, 0);
	lua_pushboolean(L, false);
	return 1;
}

static int extension_show_preview(lua_State *L) {
	dmLogDebug("show_preview");
	utils::check_arg_count(L, 0);
	return 0;
}

static const luaL_reg extension_functions[] = {
	{"enable_debug", extension_enable_debug},
	{"init", extension_init},
	{"start", extension_start},
	{"stop", extension_stop},
	{"mux_audio_video", extension_mux_audio_video},
	{"capture_frame", extension_capture_frame},
	{"get_info", extension_get_info},
	{"is_recording", extension_is_recording},
	{"is_preview_available", extension_is_preview_available},
    {"show_preview", extension_show_preview},
	{0, 0}
};

dmExtension::Result APP_INITIALIZE(dmExtension::AppParams *params) {
	return dmExtension::RESULT_OK;
}

dmExtension::Result APP_FINALIZE(dmExtension::AppParams *params) {
	return dmExtension::RESULT_OK;
}

dmExtension::Result INITIALIZE(dmExtension::Params *params) {
	luaL_register(params->m_L, EXTENSION_NAME_STRING, extension_functions);
	lua_pop(params->m_L, 1);
	sr = new ScreenRecorder();
	return dmExtension::RESULT_OK;
}

dmExtension::Result UPDATE(dmExtension::Params *params) {
	utils::execute_tasks(params->m_L);
	return dmExtension::RESULT_OK;
}

dmExtension::Result FINALIZE(dmExtension::Params *params) {
	delete sr;
	return dmExtension::RESULT_OK;
}

DECLARE_DEFOLD_EXTENSION

#endif
