#include <jni.h>

extern "C" {
	void JNICALL Java_extension_screenrecorder_Lua_lua_1dmscript_1getinstance(JNIEnv *env, jobject obj, jlong L);
	void JNICALL Java_extension_screenrecorder_Lua_lua_1dmscript_1setinstance(JNIEnv *env, jobject obj, jlong L);
	jint JNICALL Java_extension_screenrecorder_Lua_lua_1registryindex(JNIEnv *env, jobject obj, jlong L);
	void JNICALL Java_extension_screenrecorder_Lua_lua_1call(JNIEnv *env, jobject obj, jlong L, jint nargs, jint nresults);
	void JNICALL Java_extension_screenrecorder_Lua_lua_1error(JNIEnv *env, jobject obj, jlong L, jstring s);
	void JNICALL Java_extension_screenrecorder_Lua_lua_1pushboolean(JNIEnv *env, jobject obj, jlong L, jboolean b);
	void JNICALL Java_extension_screenrecorder_Lua_lua_1pushinteger(JNIEnv *env, jobject obj, jlong L, jint n);
	void JNICALL Java_extension_screenrecorder_Lua_lua_1pushnil(JNIEnv *env, jobject obj, jlong L);
	void JNICALL Java_extension_screenrecorder_Lua_lua_1pushnumber(JNIEnv *env, jobject obj, jlong L, jdouble n);
	void JNICALL Java_extension_screenrecorder_Lua_lua_1pushstring(JNIEnv *env, jobject obj, jlong L, jstring s);
	jint JNICALL Java_extension_screenrecorder_Lua_lua_1gettop(JNIEnv *env, jobject obj, jlong L);
	void JNICALL Java_extension_screenrecorder_Lua_lua_1pop(JNIEnv *env, jobject obj, jlong L, jint n);
	void JNICALL Java_extension_screenrecorder_Lua_lua_1pushvalue(JNIEnv *env, jobject obj, jlong L, jint index);
	void JNICALL Java_extension_screenrecorder_Lua_lua_1newtable(JNIEnv *env, jobject obj, jlong L);
	jint JNICALL Java_extension_screenrecorder_Lua_lua_1next(JNIEnv *env, jobject obj, jlong L, jint index);
	void JNICALL Java_extension_screenrecorder_Lua_lua_1rawgeti(JNIEnv *env, jobject obj, jlong L, jint index, jint n);
	jboolean JNICALL Java_extension_screenrecorder_Lua_lua_1toboolean(JNIEnv *env, jobject obj, jlong L, jint index);
	jdouble JNICALL Java_extension_screenrecorder_Lua_lua_1tonumber(JNIEnv *env, jobject obj, jlong L, jint index);
	jlong JNICALL Java_extension_screenrecorder_Lua_lua_1topointer(JNIEnv *env, jobject obj, jlong L, jint index);
	jstring JNICALL Java_extension_screenrecorder_Lua_lua_1tostring(JNIEnv *env, jobject obj, jlong L, jint index);
	jint JNICALL Java_extension_screenrecorder_Lua_lua_1type(JNIEnv *env, jobject obj, jlong L, jint index);
	void JNICALL Java_extension_screenrecorder_Lua_lua_1settable(JNIEnv *env, jobject obj, jlong L, jint index);
	jint JNICALL Java_extension_screenrecorder_Lua_lua_1ref(JNIEnv *env, jobject obj, jlong L, jint index);
	void JNICALL Java_extension_screenrecorder_Lua_lua_1unref(JNIEnv *env, jobject obj, jlong L, jint index, jint ref);
}