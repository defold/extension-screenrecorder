#if defined(DM_PLATFORM_OSX) || defined(DM_PLATFORM_LINUX) || defined(DM_PLATFORM_WINDOWS) || defined(DM_PLATFORM_HTML5)

#define THREAD_IMPLEMENTATION
#include <thread.h>
#include "screenrecorder_private.h"
#include "desktop/screenrecorder.h"
#include "desktop/utils.h"

static ScreenRecorder *sr = NULL;
static bool is_initialized = false;
static bool is_recording = false;
int lua_script_instance = LUA_REFNIL;
static int *lua_listener = NULL;
static thread_ptr_t mux_audio_video_thread = NULL;
static thread_ptr_t stop_thread = NULL;

// Emscripten does not support threading.
#ifdef DM_PLATFORM_HTML5
	static bool is_threading_available = false;
#else
	static bool is_threading_available = true;
#endif

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

// The extension receives video frames from Defold's render target internal texture.
// This method retrives this texture's OpenGL id.
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

int ScreenRecorder_init(lua_State *L) {
	utils::check_arg_count(L, 1);
	if (is_initialized) {
		dmLogInfo("init(): The extension is already initialized.");
		return 0;
	}

	void *render_target = NULL;

	// Parse params table argument.
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

	#ifdef DM_PLATFORM_HTML5
		*sr->capture_params.async_encoding = false;
	#endif

	if (lua_script_instance != LUA_REFNIL) {
		dmScript::Unref(L, LUA_REGISTRYINDEX, lua_script_instance);
	}
	dmScript::GetInstance(L);
	lua_script_instance = dmScript::Ref(L, LUA_REGISTRYINDEX);

	utils::Event event = {	
		.name = SCREENRECORDER,
		.phase = EVENT_INIT,
		.is_error = false
	};

	is_initialized = false;
	is_recording = false;

	int w = *sr->capture_params.width;
	int h = *sr->capture_params.height;

	char error_message[utils::ERROR_MESSAGE_MAX];

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
	} else if (!sr->init(error_message)) {
		event.is_error = true;
		event.error_message = error_message;
	} else {
		is_initialized = true;
	}
	utils::dispatch_event(L, *lua_listener, lua_script_instance, &event);
	return 0;
}

int ScreenRecorder_start(lua_State *L) {
	utils::check_arg_count(L, 0);
	if (!check_is_initialized()) {
		return 0;
	}
	if (!is_recording) {
		char start_error_message[utils::ERROR_MESSAGE_MAX];
		bool success = sr->start(start_error_message);
		if (success) {
			is_recording = true;
		} else {
			char error_message[utils::ERROR_MESSAGE_MAX];
			ERROR_MESSAGE("Failed to start video recording: %s", start_error_message);
			utils::Event event = {
				.name = SCREENRECORDER,
				.phase = EVENT_INIT,
				.is_error = true,
				.error_message = error_message
			};
			utils::dispatch_event(L, *lua_listener, lua_script_instance, &event);
		}
	}
	return 0;
}

static int stop_thread_proc(void *unused) {
	char stop_error_message[utils::ERROR_MESSAGE_MAX];
	bool is_error = !sr->stop(stop_error_message);
	utils::Event event = {
		.name = SCREENRECORDER,
		.phase = EVENT_RECORDED,
		.is_error = is_error,
	};
	if (is_error) {
		char error_message[utils::ERROR_MESSAGE_MAX];
		ERROR_MESSAGE("Failed to stop video recording: %s", stop_error_message);
		event.error_message = error_message;
	}
	utils::add_task(*lua_listener, lua_script_instance, &event);
	return 0;
}

int ScreenRecorder_stop(lua_State *L) {
	utils::check_arg_count(L, 0);
	if (is_recording) {
		if (is_threading_available) {
			if (stop_thread != NULL) {
				thread_join(stop_thread);
				thread_destroy(stop_thread);
			}
			stop_thread = thread_create(stop_thread_proc, NULL, "Stop recording thread", THREAD_STACK_SIZE_DEFAULT);
		} else {
			stop_thread_proc(NULL);
		}
	}
	is_initialized = false;
	is_recording = false;
	return 0;
}

static int mux_audio_video_thread_proc(void *unused) {
	WebmWriter webm_writer;
	char error_message[utils::ERROR_MESSAGE_MAX];
	bool is_error = !webm_writer.mux_audio_video(mux_audio_video_user_data.audio_filename, mux_audio_video_user_data.video_filename, mux_audio_video_user_data.filename, error_message);

	utils::Event event = {
		.name = SCREENRECORDER,
		.phase = EVENT_MUXED,
		.is_error = is_error
	};
	if (is_error) {
		event.error_message = error_message;
	}
	utils::add_task(*lua_listener, lua_script_instance, &event);
	return 0;
}

int ScreenRecorder_mux_audio_video(lua_State *L) {
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

	if (is_threading_available) {
		mux_audio_video_thread = thread_create(mux_audio_video_thread_proc, NULL, "Mux audio and video thread", THREAD_STACK_SIZE_DEFAULT);
	} else {
		mux_audio_video_thread_proc(NULL);
	}

	return 0;
}

int ScreenRecorder_capture_frame(lua_State *L) {
	utils::check_arg_count(L, 0);
	if (is_recording) {
		char capture_frame_error_message[utils::ERROR_MESSAGE_MAX];
		bool success = sr->capture_frame(capture_frame_error_message);
		if (!success) {
			char error_message[utils::ERROR_MESSAGE_MAX];
			ERROR_MESSAGE("Failed to capture video frame: %s", capture_frame_error_message);
			utils::Event event = {
				.name = SCREENRECORDER,
				.phase = EVENT_INIT,
				.is_error = true,
				.error_message = error_message
			};
			utils::dispatch_event(L, *lua_listener, lua_script_instance, &event);
		}
	}
	return 0;
}

int ScreenRecorder_is_recording(lua_State *L) {
	utils::check_arg_count(L, 0);
	lua_pushboolean(L, is_recording);
	return 1;
}

int ScreenRecorder_is_preview_available(lua_State *L) {
	utils::check_arg_count(L, 0);
	lua_pushboolean(L, false);
	return 1;
}

int ScreenRecorder_show_preview(lua_State *L) {
	utils::check_arg_count(L, 0);
	return 0;
}

void ScreenRecorder_initialize(lua_State *L) {
	sr = new ScreenRecorder();
}

void ScreenRecorder_update(lua_State *L) {
	utils::execute_tasks(L);
}

void ScreenRecorder_finalize(lua_State *L) {
	delete sr;
}

#endif
