#if defined(DM_PLATFORM_OSX) || defined(DM_PLATFORM_LINUX) || defined(DM_PLATFORM_WINDOWS)

#include <dmsdk/sdk.h>
#include "extension.h"

typedef void (*DefoldLogInternal)(dmLogSeverity, const char *name, const char *format, ...);
typedef bool (*GetRenderTargetTextureId)(void *render_target, int *texture_id);

bool get_render_target_texture_id(void *render_target, int *texture_id) {
	dmGraphics::HTexture color_texture = dmGraphics::GetRenderTargetAttachment((dmGraphics::HRenderTarget)render_target, dmGraphics::ATTACHMENT_COLOR);
	if (color_texture == NULL) {
		dmLogError("Could not get the color attachment from render target.");
		return false;
	}
	uint *color_gl_handle = NULL;
	dmGraphics::HandleResult result = dmGraphics::GetTextureHandle(color_texture, (void **)&color_gl_handle);
	if (result != dmGraphics::HANDLE_RESULT_OK) {
		dmLogError("Could not get texture handle from render target.");
		return false;
	}
	*texture_id = *color_gl_handle;
	return true;
}

extern "C" void APP_INITIALIZE_DESKTOP(DefoldLogInternal defold_log_internal, GetRenderTargetTextureId);
extern "C" void APP_FINALIZE_DESKTOP();
extern "C" void INITIALIZE_DESKTOP(lua_State *L);
extern "C" void UPDATE_DESKTOP(lua_State *L);
extern "C" void FINALIZE_DESKTOP(lua_State *L);

dmExtension::Result APP_INITIALIZE(dmExtension::AppParams *params) {
	APP_INITIALIZE_DESKTOP(&dmLogInternal, &get_render_target_texture_id);
	return dmExtension::RESULT_OK;
}

dmExtension::Result APP_FINALIZE(dmExtension::AppParams *params) {
	APP_FINALIZE_DESKTOP();
	return dmExtension::RESULT_OK;
}

dmExtension::Result INITIALIZE(dmExtension::Params *params) {
	INITIALIZE_DESKTOP(params->m_L);
	return dmExtension::RESULT_OK;
}

dmExtension::Result UPDATE(dmExtension::Params *params) {
	UPDATE_DESKTOP(params->m_L);
	return dmExtension::RESULT_OK;
}

dmExtension::Result FINALIZE(dmExtension::Params *params) {
	FINALIZE_DESKTOP(params->m_L);
	return dmExtension::RESULT_OK;
}

DECLARE_DEFOLD_EXTENSION

#endif
