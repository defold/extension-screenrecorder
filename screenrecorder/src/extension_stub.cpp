#ifndef DM_PLATFORM_ANDROID
#ifndef DM_PLATFORM_IOS

#include <dmsdk/sdk.h>
#include "extension.h"

static int stub(lua_State *L) {
	dmLogInfo(EXTENSION_NAME_STRING " extenstion is not supported on this platform.");
	return 0;
}

dmExtension::Result APP_INITIALIZE(dmExtension::AppParams* params) {
	return dmExtension::RESULT_OK;
}

dmExtension::Result APP_FINALIZE(dmExtension::AppParams* params) {
	return dmExtension::RESULT_OK;
}

dmExtension::Result INITIALIZE(dmExtension::Params* params) {
	const luaL_Reg lua_functions[] = {
		{"enable_debug", stub},
		{"init", stub},
		{"start", stub},
		{"finish", stub},
		{NULL, NULL}
	};

	luaL_openlib(params->m_L, EXTENSION_NAME_STRING, lua_functions, 1);
	return dmExtension::RESULT_OK;
}

dmExtension::Result UPDATE(dmExtension::Params* params) {
	return dmExtension::RESULT_OK;
}

dmExtension::Result FINALIZE(dmExtension::Params* params) {
	return dmExtension::RESULT_OK;
}

DECLARE_DEFOLD_EXTENSION

#endif
#endif
