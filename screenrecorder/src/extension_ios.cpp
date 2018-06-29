#if defined(DM_PLATFORM_IOS)

#include <dmsdk/sdk.h>
#include "extension.h"

extern "C" void APP_INITIALIZE_IOS();
extern "C" void APP_FINALIZE_IOS();
extern "C" void INITIALIZE_IOS(lua_State* L);
extern "C" void UPDATE_IOS(lua_State* L);
extern "C" void FINALIZE_IOS(lua_State* L);

dmExtension::Result APP_INITIALIZE(dmExtension::AppParams* params) {
	APP_INITIALIZE_IOS();
	return dmExtension::RESULT_OK;
}

dmExtension::Result APP_FINALIZE(dmExtension::AppParams* params) {
	APP_FINALIZE_IOS();
	return dmExtension::RESULT_OK;
}

dmExtension::Result INITIALIZE(dmExtension::Params* params) {
	INITIALIZE_IOS(params->m_L);
	return dmExtension::RESULT_OK;
}

dmExtension::Result UPDATE(dmExtension::Params* params) {
	UPDATE_IOS(params->m_L);
	return dmExtension::RESULT_OK;
}

dmExtension::Result FINALIZE(dmExtension::Params* params) {
	FINALIZE_IOS(params->m_L);
	return dmExtension::RESULT_OK;
}

DECLARE_DEFOLD_EXTENSION

#endif
