#if defined(DM_PLATFORM_ANDROID)

#include <dmsdk/sdk.h>

#include "java_lua.h"

extern "C" {
	JNIEXPORT jint JNICALL Java_extension_screenrecorder_Lua_lua_1registryindex(JNIEnv *env, jobject obj, jlong L) {
		return LUA_REGISTRYINDEX;
	}

	JNIEXPORT void JNICALL Java_extension_screenrecorder_Lua_lua_1call(JNIEnv *env, jobject obj, jlong L, jint nargs, jint nresults) {
		lua_call((lua_State*)L, nargs, nresults);
	}

	JNIEXPORT void JNICALL Java_extension_screenrecorder_Lua_lua_1error(JNIEnv *env, jobject obj, jlong L, jstring s) {
		// TODO
		luaL_error((lua_State*)L, "");
	}

	JNIEXPORT void JNICALL Java_extension_screenrecorder_Lua_lua_1pushboolean(JNIEnv *env, jobject obj, jlong L, jboolean b) {
		lua_pushboolean((lua_State*)L, b);
	}

	JNIEXPORT void JNICALL Java_extension_screenrecorder_Lua_lua_1pushinteger(JNIEnv *env, jobject obj, jlong L, jint n) {
		lua_pushinteger((lua_State*)L, n);
	}

	JNIEXPORT void JNICALL Java_extension_screenrecorder_Lua_lua_1pushnil(JNIEnv *env, jobject obj, jlong L) {
		lua_pushnil((lua_State*)L);
	}

	JNIEXPORT void JNICALL Java_extension_screenrecorder_Lua_lua_1pushnumber(JNIEnv *env, jobject obj, jlong L, jdouble n) {
		lua_pushnumber((lua_State*)L, n);
	}

	JNIEXPORT void JNICALL Java_extension_screenrecorder_Lua_lua_1pushstring(JNIEnv *env, jobject obj, jlong L, jstring s) {
		const char *c_str = env->GetStringUTFChars(s, 0);
		lua_pushstring((lua_State*)L, c_str);
		env->ReleaseStringUTFChars(s, c_str);
	}

	JNIEXPORT jint JNICALL Java_extension_screenrecorder_Lua_lua_1gettop(JNIEnv *env, jobject obj, jlong L) {
		return lua_gettop((lua_State*)L);
	}
	JNIEXPORT void JNICALL Java_extension_screenrecorder_Lua_lua_1pop(JNIEnv *env, jobject obj, jlong L, jint n) {
		lua_pop((lua_State*)L, n);
	}

	JNIEXPORT void JNICALL Java_extension_screenrecorder_Lua_lua_1pushvalue(JNIEnv *env, jobject obj, jlong L, jint index) {
		lua_pushvalue((lua_State*)L, index);
	}

	JNIEXPORT void JNICALL Java_extension_screenrecorder_Lua_lua_1newtable(JNIEnv *env, jobject obj, jlong L) {
		lua_newtable((lua_State*)L);
	}
	
	JNIEXPORT jint JNICALL Java_extension_screenrecorder_Lua_lua_1next(JNIEnv *env, jobject obj, jlong L, jint index) {
		return lua_next((lua_State*)L, index);
	}
	
	JNIEXPORT void JNICALL Java_extension_screenrecorder_Lua_lua_1rawgeti(JNIEnv *env, jobject obj, jlong L, jint index, jint n) {
		lua_rawgeti((lua_State*)L, index, n);
	}
	
	JNIEXPORT jboolean JNICALL Java_extension_screenrecorder_Lua_lua_1toboolean(JNIEnv *env, jobject obj, jlong L, jint index) {
		return (bool)lua_toboolean((lua_State*)L, index);
	}
	
	JNIEXPORT jdouble JNICALL Java_extension_screenrecorder_Lua_lua_1tonumber(JNIEnv *env, jobject obj, jlong L, jint index) {
		return lua_tonumber((lua_State*)L, index);
	}
	
	JNIEXPORT jlong JNICALL Java_extension_screenrecorder_Lua_lua_1topointer(JNIEnv *env, jobject obj, jlong L, jint index) {
		return (jlong)lua_topointer((lua_State*)L, index);
	}
	
	JNIEXPORT jstring JNICALL Java_extension_screenrecorder_Lua_lua_1tostring(JNIEnv *env, jobject obj, jlong L, jint index) {
		return env->NewStringUTF(lua_tostring((lua_State*)L, index));
	}
	
	JNIEXPORT jint JNICALL Java_extension_screenrecorder_Lua_lua_1type(JNIEnv *env, jobject obj, jlong L, jint index) {
		return lua_type((lua_State*)L, index);
	}
	
	JNIEXPORT void JNICALL Java_extension_screenrecorder_Lua_lua_1settable(JNIEnv *env, jobject obj, jlong L, jint index) {
		lua_settable((lua_State*)L, index);
	}
	
	JNIEXPORT jint JNICALL Java_extension_screenrecorder_Lua_lua_1ref(JNIEnv *env, jobject obj, jlong L, jint index) {
		return luaL_ref((lua_State*)L, index);
	}
	
	JNIEXPORT void JNICALL Java_extension_screenrecorder_Lua_lua_1unref(JNIEnv *env, jobject obj, jlong L, jint index, jint ref) {
		luaL_unref((lua_State*)L, index, ref);
	}
	
}

#endif