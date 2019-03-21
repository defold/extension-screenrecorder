package extension.screenrecorder;

import android.text.TextUtils;
import android.util.Log;

import java.util.ArrayList;
import java.util.Hashtable;
import java.util.List;
import java.util.Map;

abstract class JavaFunction {
	abstract public int invoke(long L);
}

abstract class Utils {
	private static ArrayList<JavaFunction> tasks = new ArrayList<JavaFunction>();

	static int newRef(long L, int index) {
		Lua.pushvalue(L, index);
		return Lua.ref(L, Lua.REF_OWNER);
	}

	static void deleteRef(long L, int ref) {
		if (ref > 0) {
			Lua.unref(L, Lua.REF_OWNER, ref);
		}
	}

	private static String TAG = "debug";

	static void setTag(String tag) {
		TAG = tag;
	}

	static void log(String message) {
		Log.i(TAG, message);
	}

	static void checkArgCount(long L, int countExact) {
		int count = Lua.gettop(L);
		if (count != countExact) {
			Log.e(TAG, "This function requires " + countExact + " arguments. Got " + count + ".");
			Lua.error(L, "This function requires " + countExact + " arguments. Got " + count + ".");
		}
	}

	static void checkArgCount(long L, int countFrom, int countTo) {
		int count = Lua.gettop(L);
		if ((count < countFrom) || (count > countTo)) {
			Log.e(TAG, "This function requires from " + countFrom + " to " + countTo + " arguments. Got " + count + ".");
			Lua.error(L, "This function requires from " + countFrom + " to " + countTo + " arguments. Got " + count + ".");
		}
	}

	static void deleteRefIfNotNil(long L, int ref) {
		if ((ref != Lua.REFNIL) && (ref != Lua.NOREF)) {
			deleteRef(L, ref);
		}
	}

	static void put(Hashtable<Object, Object> hastable, String key, Object value) {
		if (value != null) {
			hastable.put(key, value);
		}
	}

	static Hashtable<Object, Object> newEvent(String name) {
		Hashtable<Object, Object> event = new Hashtable<Object, Object>();
		event.put("name", name);
		return event;
	}

	static void dispatchEvent(final int listener, final Integer script_instance, final Hashtable<Object, Object> event) {
		dispatchEvent(listener, script_instance, event, false);
	}

	static void dispatchEvent(final Integer listener, final Integer script_instance, final Hashtable<Object, Object> event, final boolean shouldDeleteRef) {
		if ((listener == null) || (listener == Lua.REFNIL) || (listener == Lua.NOREF) || (script_instance == null) || (script_instance == Lua.REFNIL) || (script_instance == Lua.NOREF)) {
			return;
		}
		JavaFunction task = new JavaFunction() {
			public int invoke(long L) {
				Lua.rawget(L, Lua.REF_OWNER, listener);
				Lua.rawget(L, Lua.REF_OWNER, script_instance);
				Lua.dmscript_setinstance(L);
				pushHashtable(L, event);
				Lua.call(L, 1, 0);
				if (shouldDeleteRef) {
					deleteRef(L, listener);
					deleteRef(L, script_instance);
				}
				return 0;
			}
		};
		tasks.add(task);
	}

	static void executeTasks(long L) {
		while (!tasks.isEmpty()) {
			JavaFunction task = tasks.remove(0);
			task.invoke(L);
		}
	}

	static class LuaLightuserdata {
		long pointer;

		LuaLightuserdata(long pointer) {
			this.pointer = pointer;
		}
	}

	static class LuaValue {
		int reference = Lua.REFNIL;

		LuaValue(long L, int index) {
			reference = newRef(L, index);
		}

		void delete(long L) {
			if (reference != Lua.REFNIL) {
				deleteRef(L, reference);
			}
		}
	}

	interface LuaPushable {
		void push(long L);
	}

	static class Table {
		private long L;
		private int index;
		private Hashtable<Object, Object> hashtable;
		private Scheme scheme;

		Table (long L, int index) {
			this.L = L;
			this.index = index;
		}

		Table parse(Scheme scheme) {
			this.scheme = scheme;
			hashtable = toHashtable(L, index, null);
			return this;
		}

		Boolean getBoolean(String path, Boolean defaultValue) {
			Boolean result = getBoolean(path);
			if (result != null) {
				return result;
			} else {
				return defaultValue;
			}
		}

		Boolean getBoolean(String path) {
			if ((get(path) != null) && (get(path) instanceof Boolean)) {
				return (Boolean) get(path);
			} else {
				return null;
			}
		}

		String getString(String path, String defaultValue) {
			String result = getString(path);
			if (result != null) {
				return result;
			} else {
				return defaultValue;
			}
		}

		String getString(String path) {
			if ((get(path) != null) && (get(path) instanceof String)) {
				return (String) get(path);
			} else {
				return null;
			}
		}

		String getStringNotNull(String path) {
			String result = getString(path);
			if (result != null) {
				return result;
			} else {
				Log.e(TAG, "ERROR: Table's property '" + path + "' is not a string.");
				Lua.error(L, "ERROR: Table's property '" + path + "' is not a string.");
				return null;
			}
		}

		Double getDouble(String path, double defaultValue) {
			Double result = getDouble(path);
			if (result != null) {
				return result;
			} else {
				return defaultValue;
			}
		}

		Double getDouble(String path) {
			if ((get(path) != null) && (get(path) instanceof Double)) {
				return (Double) get(path);
			} else {
				return null;
			}
		}

		Double getDoubleNotNull(String path) {
			Double result = getDouble(path);
			if (result != null) {
				return result;
			} else {
				Log.e(TAG, "ERROR: Table's property '" + path + "' is not a number.");
				Lua.error(L, "ERROR: Table's property '" + path + "' is not a number.");
				return null;
			}
		}

		Integer getInteger(String path, int defaultValue) {
			Integer result = getInteger(path);
			if (result != null) {
				return result;
			} else {
				return defaultValue;
			}
		}

		Integer getInteger(String path) {
			if ((get(path) != null) && (get(path) instanceof Double)) {
				return ((Double) get(path)).intValue();
			} else {
				return null;
			}
		}

		Integer getIntegerNotNull(String path) {
			Integer result = getInteger(path);
			if (result != null) {
				return result;
			} else {
				Log.e(TAG, "ERROR: Table's property '" + path + "' is not a number.");
				Lua.error(L, "ERROR: Table's property '" + path + "' is not a number.");
				return null;
			}
		}

		Long getLong(String path, long defaultValue) {
			Long result = getLong(path);
			if (result != null) {
				return result;
			} else {
				return defaultValue;
			}
		}

		Long getLong(String path) {
			if ((get(path) != null) && (get(path) instanceof Double)) {
				return ((Double) get(path)).longValue();
			} else {
				return null;
			}
		}

		Long getLongNotNull(String path) {
			Long result = getLong(path);
			if (result != null) {
				return result;
			} else {
				Log.e(TAG, "ERROR: Table's property '" + path + "' is not a number.");
				Lua.error(L, "ERROR: Table's property '" + path + "' is not a number.");
				return null;
			}
		}

		byte[] getByteArray(String path, byte[] defaultValue) {
			byte[] result = getByteArray(path);
			if (result != null) {
				return result;
			} else {
				return defaultValue;
			}
		}

		byte[] getByteArray(String path) {
			if ((get(path) != null) && (get(path) instanceof byte[])) {
				return (byte[]) get(path);
			} else {
				return null;
			}
		}

		byte[] getByteArrayNotNull(String path) {
			byte[] result = getByteArray(path);
			if (result != null) {
				return result;
			} else {
				Log.e(TAG, "ERROR: Table's property '" + path + "' is not a byte array.");
				Lua.error(L, "ERROR: Table's property '" + path + "' is not a byte array.");
				return null;
			}
		}

		LuaLightuserdata getLightuserdata(String path, Long defaultValue) {
			LuaLightuserdata result = getLightuserdata(path);
			if (result != null) {
				return result;
			} else {
				return new LuaLightuserdata(defaultValue);
			}
		}

		LuaLightuserdata getLightuserdata(String path) {
			if ((get(path) != null) && (get(path) instanceof LuaLightuserdata)) {
				return (LuaLightuserdata) get(path);
			} else {
				return null;
			}
		}

		LuaLightuserdata getLightuserdataNotNull(String path) {
			LuaLightuserdata result = getLightuserdata(path);
			if (result != null) {
				return result;
			} else {
				Log.e(TAG, "ERROR: Table's property '" + path + "' is not a lightuserdata.");
				Lua.error(L, "ERROR: Table's property '" + path + "' is not a lightuserdata.");
				return null;
			}
		}

		Integer getFunction(String path, Integer defaultValue) {
			Integer result = getFunction(path);
			if (result != null) {
				return result;
			} else {
				return defaultValue;
			}
		}

		Integer getFunction(String path) {
			if ((get(path) != null) && (get(path) instanceof Integer)) {
				return (Integer) get(path);
			} else {
				return null;
			}
		}

		@SuppressWarnings("unchecked")
		Hashtable<Object, Object> getTable(String path) {
			if ((get(path) != null) && (get(path) instanceof Hashtable)) {
				return (Hashtable<Object, Object>) get(path);
			} else {
				return null;
			}
		}

		Object get(String path) {
			if (path.isEmpty()) {
				return hashtable;
			} else {
				Object current = null;
				for (String p : path.split("\\.")) {
					if (current == null) {
						current = hashtable.get(p);
					} else if (current instanceof Hashtable) {
						Hashtable h = (Hashtable) current;
						current = h.get(p);
					}
				}
				return current;
			}
		}

		private Object toValue(long L, int index, ArrayList<String> pathList) {
			if ((index < 0) && (index > Lua.registryindex(L))) {
				index = Lua.gettop(L) + index + 1;
			}
			Object o = null;
			if (scheme == null) {
				switch (Lua.type(L, index)) {
					case STRING:
						o = Lua.tostring(L, index);
						break;
					case NUMBER:
						o = Lua.tonumber(L, index);
						break;
					case BOOLEAN:
						o = Lua.toboolean(L, index);
						break;
					case LIGHTUSERDATA:
						o = new LuaLightuserdata(Lua.topointer(L, index));
						break;
					case TABLE:
						o = toHashtable(L, index, pathList);
						break;
				}
			} else {
				String path = TextUtils.join(".", pathList);
				Object rule = scheme.get(path);
				switch (Lua.type(L, index)) {
					case STRING:
						if (rule == Lua.Type.STRING) {
							o = Lua.tostring(L, index);
						} else if (rule == Scheme.LuaTypeNumeric) {
							String value = Lua.tostring(L, index);
							boolean isNumeric = true;
							try {
								Double.parseDouble(value); // Not ignored
							} catch(NumberFormatException e) {
								isNumeric = false;
							}
							if (isNumeric) {
								o = value;
							}
						}
						break;
					case NUMBER:
						if ((rule == Lua.Type.NUMBER) || (rule == Scheme.LuaTypeNumeric)) {
							o = Lua.tonumber(L, index);
						}
						break;
					case BOOLEAN:
						if (rule == Lua.Type.BOOLEAN) {
							o = Lua.toboolean(L, index);
						}
						break;
					case LIGHTUSERDATA:
						if (rule == Lua.Type.LIGHTUSERDATA) {
							o = new LuaLightuserdata(Lua.topointer(L, index));
						}
						break;
					case USERDATA:
						if (rule == Lua.Type.USERDATA) {
							o = Lua.topointer(L, index);
						}
						break;
					case FUNCTION:
						if (rule == Lua.Type.FUNCTION) {
							o = newRef(L, index);
						}
						break;
					case TABLE:
						if (rule == Lua.Type.TABLE) {
							o = toHashtable(L, index, pathList);
						}
				}
			}
			return o;
		}

		private Hashtable<Object, Object> toHashtable(long L, int index, ArrayList<String> pathList) {
			if ((index < 0) && (index > Lua.registryindex(L))) {
				index = Lua.gettop(L) + index + 1;
			}

			Hashtable<Object, Object> result = new Hashtable<Object, Object>();
			if (Lua.type(L, index) != Lua.Type.TABLE) {
				return result;
			}
			Lua.pushnil(L);

			ArrayList<String> path = pathList != null ? pathList : new ArrayList<String>();
			for(; Lua.next(L, index); Lua.pop(L, 1)) {
				Object key = null;
				if (Lua.type(L, -2) == Lua.Type.STRING) {
					key = Lua.tostring(L, -2);
					path.add((String) key);
				} else if (Lua.type(L, -2) == Lua.Type.NUMBER) {
					key = Lua.tonumber(L, -2);
					path.add("#");
				}
				if (key != null) {
					Object value = toValue(L, -1, path);
					if (value != null) {
						result.put(key, value);
					}
					path.remove(path.size() - 1);
				}
			}

			return result;
		}
	}

	static class Scheme {
		Hashtable<String, Object> scheme = new Hashtable<String, Object>();

		final static Integer LuaTypeNumeric = 1000;

		Scheme string(String path) {
			scheme.put(path, Lua.Type.STRING);
			return this;
		}

		Scheme number(String path) {
			scheme.put(path, Lua.Type.NUMBER);
			return this;
		}

		Scheme bool(String path) {
			scheme.put(path, Lua.Type.BOOLEAN);
			return this;
		}

		Scheme table(String path) {
			scheme.put(path, Lua.Type.TABLE);
			return this;
		}

		Scheme function(String path) {
			scheme.put(path, Lua.Type.FUNCTION);
			return this;
		}

		Scheme lightuserdata(String path) {
			scheme.put(path, Lua.Type.LIGHTUSERDATA);
			return this;
		}

		Scheme userdata(String path) {
			scheme.put(path, Lua.Type.USERDATA);
			return this;
		}

		Scheme numeric(String path) {
			scheme.put(path, LuaTypeNumeric);
			return this;
		}

		Object get(String path) {
			return scheme.get(path);
		}
	}

	@SuppressWarnings("unchecked")
	static void pushValue(long L, Object object) {
		if(object instanceof String) {
			Lua.pushstring(L, (String)object);
		} else if(object instanceof Integer) {
			Lua.pushinteger(L, (Integer)object);
		} else if(object instanceof Long) {
			Lua.pushnumber(L, ((Long)object).doubleValue());
		} else if(object instanceof Double) {
			Lua.pushnumber(L, (Double)object);
		} else if(object instanceof Boolean) {
			Lua.pushboolean(L, (Boolean)object);
		//} else if(object instanceof byte[]) {
		//	Lua.pushstring(L, (byte[])object);
		//} else if(object instanceof LuaLightuserdata) {
			//pushBaseDir(L, ((LuaLightuserdata) object).pointer);
		} else if(object instanceof LuaValue) {
			LuaValue value = (LuaValue) object;
			Lua.ref(L, value.reference);
			value.delete(L);
		} else if(object instanceof LuaPushable) {
			((LuaPushable) object).push(L);
		} else if(object instanceof List) {
			Hashtable<Object, Object> hashtable = new Hashtable<Object, Object>();
			int i = 1;
			for (Object o : (List)object) {
				hashtable.put(i, o);
			}
			pushHashtable(L, hashtable);
		} else if(object instanceof Hashtable) {
			pushHashtable(L, (Hashtable)object);
		} else {
			Lua.pushnil(L);
		}
	}

	static void pushHashtable(long L, Hashtable<Object, Object> hashtable) {
		if (hashtable == null) {
			Lua.newtable(L);
		} else {
			Lua.newtable(L, 0, hashtable.size());
			int tableIndex = Lua.gettop(L);
			for (Object o : hashtable.entrySet()) {
				Map.Entry entry = (Map.Entry) o;
				pushValue(L, entry.getKey());
				pushValue(L, entry.getValue());
				Lua.settable(L, tableIndex);
			}
		}
	}
}
