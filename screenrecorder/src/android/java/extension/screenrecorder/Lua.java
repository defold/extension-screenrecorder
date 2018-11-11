package extension.screenrecorder;

@SuppressWarnings("JniMissingFunction")
class Lua {
	private static native int lua_registryindex(long L);

	private static native void lua_error(long L, String s);

	private static native void lua_call(long L, int nargs, int nresults);

	private static native void lua_pushboolean(long L, boolean b);

	private static native void lua_pushinteger(long L, int n);

	private static native void lua_pushnil(long L);

	private static native void lua_pushnumber(long L, double n);

	private static native void lua_pushstring(long L, String s);

	private static native boolean lua_toboolean(long L, int index);

	private static native double lua_tonumber(long L, int index);

	private static native long lua_topointer(long L, int index);

	private static native String lua_tostring(long L, int index);

	private static native int lua_type(long L, int index);

	private static native int lua_gettop(long L);

	private static native void lua_pop(long L, int n);

	private static native void lua_pushvalue(long L, int index);

	private static native void lua_newtable(long L);

	private static native boolean lua_next(long L, int index);

	private static native void lua_rawgeti(long L, int index, int n);

	private static native void lua_settable(long L, int index);

	private static native int lua_ref(long L, int index);

	private static native void lua_unref(long L, int index, int ref);

	static int registryindex(long L) {
		return lua_registryindex(L);
	}

	static void error(long L, String s) {
		lua_error(L, s);
	}

	static void call(long L, int nargs, int nresults) {
		lua_call(L, nargs, nresults);
	}

	static void pushboolean(long L, boolean b) {
		lua_pushboolean(L, b);
	}

	static void pushinteger(long L, int n) {
		lua_pushinteger(L, n);
	}

	static void pushnil(long L) {
		lua_pushnil(L);
	}

	static void pushnumber(long L, double number) {
		lua_pushnumber(L, number);
	}

	static void pushstring(long L, String s) {
		lua_pushstring(L, s);
	}

	static int gettop(long L) {
		return lua_gettop(L);
	}

	static void pop(long L, int n) {
		lua_pop(L, n);
	}

	static void pushvalue(long L, int index) {
		lua_pushvalue(L, index);
	}

	static void newtable(long L) {
		lua_newtable(L);
	}

	static void newtable(long L, int array_size, int hash_size) {
		lua_newtable(L);
	}

	static boolean next(long L, int index) {
		return lua_next(L, index);
	}

	static void rawget(long L, int index, int n) {
		lua_rawgeti(L, index, n);
	}

	static boolean toboolean(long L, int index) {
		return lua_toboolean(L, index);
	}

	static double tonumber(long L, int index) {
		return lua_tonumber(L, index);
	}

	static long topointer(long L, int index) {
		return lua_topointer(L, index);
	}

	static String tostring(long L, int index) {
		return lua_tostring(L, index);
	}

	static Type type(long L, int index) {
		return Type.values()[lua_type(L, index)];
	}

	static void settable(long L, int index) {
		lua_settable(L, index);
	}

	static int ref(long L, int index) {
		return lua_ref(L, index);
	}

	static void unref(long L, int index, int ref) {
		lua_unref(L, index, ref);
	}

	enum Type {
		NIL,
		BOOLEAN,
		LIGHTUSERDATA,
		NUMBER,
		STRING,
		TABLE,
		FUNCTION,
		USERDATA,
		THREAD
	}

	static final int REF_OWNER = -10000;
	static final int REFNIL = -1;
	static final int NOREF = -2;
}
