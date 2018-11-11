#include "screenrecorder_private.h"

// This is the entry point of the extension. It defines Lua API of the extension.
// All ScreenRecorder_* functions are implemented separately for iOS, Android and Desktop platforms. 

static const luaL_reg lua_functions[] = {
	{"init", ScreenRecorder_init},
	{"start", ScreenRecorder_start},
	{"stop", ScreenRecorder_stop},
	{"mux_audio_video", ScreenRecorder_mux_audio_video},
	{"capture_frame", ScreenRecorder_capture_frame},
	{"is_recording", ScreenRecorder_is_recording},
	{"is_preview_available", ScreenRecorder_is_preview_available},
    {"show_preview", ScreenRecorder_show_preview},
	{0, 0}
};

dmExtension::Result ScreenRecorder_AppInitialize(dmExtension::AppParams *params) {
	return dmExtension::RESULT_OK;
}

dmExtension::Result ScreenRecorder_AppFinalize(dmExtension::AppParams *params) {
	return dmExtension::RESULT_OK;
}

dmExtension::Result ScreenRecorder_Initialize(dmExtension::Params *params) {
	luaL_register(params->m_L, EXTENSION_NAME_STRING, lua_functions);
	lua_pop(params->m_L, 1);
	ScreenRecorder_initialize(params->m_L);
	return dmExtension::RESULT_OK;
}

dmExtension::Result ScreenRecorder_Update(dmExtension::Params *params) {
	ScreenRecorder_update(params->m_L);
	return dmExtension::RESULT_OK;
}

dmExtension::Result ScreenRecorder_Finalize(dmExtension::Params *params) {
	ScreenRecorder_finalize(params->m_L);
	return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(EXTENSION_NAME, EXTENSION_NAME_STRING, ScreenRecorder_AppInitialize, ScreenRecorder_AppFinalize, ScreenRecorder_Initialize, ScreenRecorder_Update, 0, ScreenRecorder_Finalize)