#ifndef dmsdk_sdk_h
#define dmsdk_sdk_h

#define LUA_COMPAT_MODULE 1

#ifdef __OBJC__
	#import <Foundation/Foundation.h>
	#import <UIKit/UIKit.h>
#endif

#ifdef DM_PLATFORM_ANDROID
	#include <EGL/egl.h>
	#include <EGL/eglext.h>
#endif

#include <lua/lua.h>
#include <lua/lauxlib.h>

#include <dmsdk/dlib/buffer.h>

#define luaL_reg luaL_Reg

// DM_DECLARE_EXTENSION(symbol, name, app_init, app_final, init, update, on_event, final)
#define DM_DECLARE_EXTENSION(symbol, name, app_init, app_final, init, update, on_event, final) ;

namespace dmExtension {
	typedef int Result;
	const int RESULT_OK = 1;
	struct Params {
		lua_State *m_L;
	};
	struct AppParams {
		lua_State *m_L;
	};
};

namespace dmScript {
	void GetInstance(lua_State *L);
	void SetInstance(lua_State *L);
	int Ref(lua_State *L, int table);
	int Unref(lua_State *L, int table, int reference);
};

namespace dmGraphics {
	typedef enum ATTACHEMENTS {
		ATTACHMENT_COLOR
	} ATTACHEMENTS;
	typedef enum HandleResult {
		HANDLE_RESULT_OK,
		HANDLE_RESULT_ERROR
	} HandleResult;
	typedef void *HRenderTarget;
	typedef void *HTexture;
	HTexture GetRenderTargetAttachment(HRenderTarget render_target, ATTACHEMENTS);
	HandleResult GetTextureHandle(HTexture texture, void **handle);
	#ifdef __OBJC__
		UIWindow *GetNativeiOSUIWindow();
		UIView *GetNativeiOSUIView();
	#endif
	#ifdef DM_PLATFORM_ANDROID
		EGLSurface GetNativeAndroidEGLSurface();
		jobject GetNativeAndroidActivity();
		JavaVM* GetNativeAndroidJavaVM();
	#endif
}

#endif
