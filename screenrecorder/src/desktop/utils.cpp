#if defined(DM_PLATFORM_OSX) || defined(DM_PLATFORM_LINUX) || defined(DM_PLATFORM_WINDOWS)

#include <string>
#include <queue>

#include "utils.h"

static std::queue<utils::Task> tasks;

static bool is_debug = false;
static const char *EVENT_NAME = "name";
static const char *EVENT_PHASE = "phase";
static const char *EVENT_IS_ERROR = "is_error";
static const char *EVENT_ERROR_MESSAGE = "error_message";

static char *copy_string(const char *source) {
	if (source != NULL) {
		size_t length = strlen(source) + 1;
		char *destination = new char[length];
		strncpy(destination, source, length);
		destination[length - 1] = 0;
		return destination;
	}
	return NULL;
}

namespace utils {
	uint64_t get_time() {
		#ifdef _SYS_TIME_H_
			timeval tv;
			gettimeofday(&tv, 0);
			return ((uint64_t) tv.tv_sec) * 1000000U + tv.tv_usec;
		#else
			return 0;
		#endif
	}

	void enable_debug() {
		is_debug = true;
	}

	void check_arg_count(lua_State *L, int count_exact) {
		int count = lua_gettop(L);
		if (count != count_exact) {
			luaL_error(L, "This function requires %d arguments. Got %d.", count_exact, count);
		}
	}

	void check_arg_count(lua_State *L, int count_from, int count_to) {
		int count = lua_gettop(L);
		if (count < count_from || count > count_to) {
			luaL_error(L, "This function requires from %d to %d arguments. Got %d.", count_from, count_to, count);
		}
	}

	void get_table(lua_State *L, int index) {
		if (!lua_istable(L, index)) {
			luaL_error(L, "Missing parameters table argument.");
		}
		lua_pushvalue(L, index);
	}

	// String.
	void table_get_string_value(lua_State *L, const char *key, char **value, const char *default_value, bool not_null) {
		if (*value != NULL) {
			delete []*value;
			*value = NULL;
		}
		lua_getfield(L, -1, key);
		int value_type = lua_type(L, -1);
		if (value_type == LUA_TSTRING) {
			size_t length;
			const char *lua_string = lua_tolstring(L, -1, &length);
			++length; // Adding null terminator.
			*value = new char[length];
			strncpy(*value, lua_string, length);
			(*value)[length - 1] = 0;
		} else if (value_type == LUA_TNIL && default_value != NULL) {
			size_t length = strlen(default_value) + 1; // Adding null terminator.
			*value = new char[length];
			strncpy(*value, default_value, length);
			(*value)[length - 1] = 0;
		}
		lua_pop(L, 1);
		if (*value == NULL && not_null) {
			luaL_error(L, "Table's property %s is not a string.", key);
		}
	}

	void table_get_string(lua_State *L, const char *key, char **value) {
		table_get_string_value(L, key, value, NULL, false);
	}

	void table_get_string(lua_State *L, const char *key, char **value, const char *default_value) {
		if (default_value != NULL) {
			table_get_string_value(L, key, value, default_value, false);
		} else {
			dmLogError("table_get_string: default value is NULL for key %s", key);
		}
	}

	void table_get_string_not_null(lua_State *L, const char *key, char **value) {
		table_get_string_value(L, key, value, NULL, true);
	}

	// Integer.
	void table_get_integer_value(lua_State *L, const char *key, int **value, int *default_value, bool not_null) {
		if (*value != NULL) {
			delete *value;
			*value = NULL;
		}
		lua_getfield(L, -1, key);
		int value_type = lua_type(L, -1);
		if (value_type == LUA_TNUMBER) {
			lua_Number lua_number = lua_tonumber(L, -1);
			*value = new int((int)lua_number);
		} else if (value_type == LUA_TNIL && default_value != NULL) {
			*value = new int(*default_value);
		}
		lua_pop(L, 1);
		if (*value == NULL && not_null) {
			luaL_error(L, "Table's property %s is not a number.", key);
		}
	}

	void table_get_integer(lua_State *L, const char *key, int **value) {
		table_get_integer_value(L, key, value, NULL, false);
	}

	void table_get_integer(lua_State *L, const char *key, int **value, int default_value) {
		table_get_integer_value(L, key, value, &default_value, false);
	}

	void table_get_integer_not_null(lua_State *L, const char *key, int **value) {
		table_get_integer_value(L, key, value, NULL, true);
	}

	// Double.
	void table_get_double_value(lua_State *L, const char *key, double **value, double *default_value, bool not_null) {
		if (*value != NULL) {
			delete *value;
			*value = NULL;
		}
		lua_getfield(L, -1, key);
		int value_type = lua_type(L, -1);
		if (value_type == LUA_TNUMBER) {
			lua_Number lua_number = lua_tonumber(L, -1);
			*value = new double(lua_number);
		} else if (value_type == LUA_TNIL && default_value != NULL) {
			*value = new double(*default_value);
		}
		lua_pop(L, 1);
		if (*value == NULL && not_null) {
			luaL_error(L, "Table's property %s is not a number.", key);
		}
	}

	void table_get_double(lua_State *L, const char *key, double **value) {
		table_get_double_value(L, key, value, NULL, false);
	}

	void table_get_double(lua_State *L, const char *key, double **value, double default_value) {
		table_get_double_value(L, key, value, &default_value, false);
	}

	void table_get_double_not_null(lua_State *L, const char *key, double **value) {
		table_get_double_value(L, key, value, NULL, true);
	}

	// Boolean.
	void table_get_boolean_value(lua_State *L, const char *key, bool **value, bool *default_value, bool not_null) {
		if (*value != NULL) {
			delete *value;
			*value = NULL;
		}
		lua_getfield(L, -1, key);
		int value_type = lua_type(L, -1);
		if (value_type == LUA_TBOOLEAN) {
			bool lua_boolean = lua_toboolean(L, -1);
			*value = new bool(lua_boolean);
		} else if (value_type == LUA_TNIL && default_value != NULL) {
			*value = new bool(*default_value);
		}
		lua_pop(L, 1);
		if (*value == NULL && not_null) {
			luaL_error(L, "Table's property %s is not a boolean.", key);
		}
	}

	void table_get_boolean(lua_State *L, const char *key, bool **value) {
		table_get_boolean_value(L, key, value, NULL, false);
	}

	void table_get_boolean(lua_State *L, const char *key, bool **value, bool default_value) {
		table_get_boolean_value(L, key, value, &default_value, false);
	}

	void table_get_boolean_not_null(lua_State *L, const char *key, bool **value) {
		table_get_boolean_value(L, key, value, NULL, true);
	}

	// Function.
	void table_get_function_value(lua_State *L, const char *key, int **value, int *default_value, bool not_null) {
		if (*value != NULL) {
			delete *value;
			*value = NULL;
		}
		lua_getfield(L, -1, key);
		int value_type = lua_type(L, -1);
		if (value_type == LUA_TFUNCTION) {
			int lua_ref = luaL_ref(L, LUA_REGISTRYINDEX);
			*value = new int(lua_ref);
		} else if (value_type == LUA_TNIL && default_value != NULL) {
			*value = new int(*default_value);
		}
		lua_pop(L, 1);
		if (*value == NULL && not_null) {
			luaL_error(L, "Table's property %s is not a function.", key);
		}
	}

	void table_get_function(lua_State *L, const char *key, int **value) {
		table_get_function_value(L, key, value, NULL, false);
	}

	void table_get_function(lua_State *L, const char *key, int **value, int default_value) {
		table_get_function_value(L, key, value, &default_value, false);
	}

	void table_get_function_not_null(lua_State *L, const char *key, int **value) {
		table_get_function_value(L, key, value, NULL, true);
	}

	// Lightuserdata.
	void table_get_lightuserdata_value(lua_State *L, const char *key, void **value, void *default_value, bool not_null) {
		lua_getfield(L, -1, key);
		int value_type = lua_type(L, -1);
		if (value_type == LUA_TLIGHTUSERDATA) {
			*value = lua_touserdata(L, -1);
		} else if (value_type == LUA_TNIL && default_value != NULL) {
			*value = default_value;
		}
		lua_pop(L, 1);
		if (*value == NULL && not_null) {
			luaL_error(L, "Table's property %s is not a lightuserdata.", key);
		}
	}

	void table_get_lightuserdata(lua_State *L, const char *key, void **value) {
		table_get_lightuserdata_value(L, key, value, NULL, false);
	}

	void table_get_lightuserdata(lua_State *L, const char *key, void **value, void *default_value) {
		table_get_lightuserdata_value(L, key, value, default_value, false);
	}

	void table_get_lightuserdata_not_null(lua_State *L, const char *key, void **value) {
		table_get_lightuserdata_value(L, key, value, NULL, true);
	}

	void table_set_string_field(lua_State *L, const char *key, const char *value) {
		if (value != NULL) {
			lua_pushstring(L, value);
			lua_setfield(L, -2, key);
		}
	}

	void table_set_boolean_field(lua_State *L, const char *key, bool value) {
		lua_pushboolean(L, value);
		lua_setfield(L, -2, key);
	}

	void dispatch_event(lua_State *L, int listener, Event *event) {
		if (listener == LUA_REFNIL || listener == LUA_NOREF) {
			return;
		}
		lua_rawgeti(L, LUA_REGISTRYINDEX, listener);
		lua_newtable(L);
		table_set_string_field(L, EVENT_NAME, event->name);
		table_set_string_field(L, EVENT_PHASE, event->phase);
		table_set_boolean_field(L, EVENT_IS_ERROR, event->is_error);
		table_set_string_field(L, EVENT_ERROR_MESSAGE, event->error_message);
		lua_call(L, 1, 0);
	}

	void add_task(int listener, Event *event) {
		Event event_copy = {
			.name = copy_string(event->name),
			.phase = copy_string(event->phase),
			.is_error = event->is_error,
			.error_message = copy_string(event->error_message)
		};
		tasks.push(std::make_pair(listener, event_copy));
	}

	void execute_tasks(lua_State *L) {
		while (!tasks.empty()) {
			Task task = tasks.front();
			int listener = task.first;
			Event *event = &task.second;
			dispatch_event(L, listener, event);
			delete []event->name;
			delete []event->phase;
			delete []event->error_message;
			tasks.pop();
		}
	}
}

#endif