#ifndef utils_h
#define utils_h

#include <utility>

#if defined(__linux__) || defined(__APPLE__)
	#include <sys/time.h>
#endif

#include "sdk.h"
#include "../extension.h"

#define ERROR_MESSAGE(format, ...) snprintf(error_message, 2048, format, ##__VA_ARGS__)

namespace utils {
	const int ERROR_MESSAGE_MAX = 2048;
	struct Event {
		const char *name;
		const char *phase;
		bool is_error;
		const char *error_message;
	};
	typedef std::pair<int, Event> Task;
	uint64_t get_time();
	void enable_debug();
	void check_arg_count(lua_State *L, int count_exact);
	void check_arg_count(lua_State *L, int count_from, int count_to);
	void get_table(lua_State *L, int index);

	void table_get_string(lua_State *L, const char *key, char **value);
	void table_get_string(lua_State *L, const char *key, char **value, const char *default_value);
	void table_get_string_not_null(lua_State *L, const char *key, char **value);

	void table_get_integer(lua_State *L, const char *key, int **value);
	void table_get_integer(lua_State *L, const char *key, int **value, int default_value);
	void table_get_integer_not_null(lua_State *L, const char *key, int **value);

	void table_get_double(lua_State *L, const char *key, double **value);
	void table_get_double(lua_State *L, const char *key, double **value, double default_value);
	void table_get_double_not_null(lua_State *L, const char *key, double **value);

	void table_get_boolean(lua_State *L, const char *key, bool **value);
	void table_get_boolean(lua_State *L, const char *key, bool **value, bool default_value);
	void table_get_boolean_not_null(lua_State *L, const char *key, bool **value);

	void table_get_function(lua_State *L, const char *key, int **value);
	void table_get_function(lua_State *L, const char *key, int **value, int default_value);
	void table_get_function_not_null(lua_State *L, const char *key, int **value);

	void table_get_lightuserdata(lua_State *L, const char *key, void **value);
	void table_get_lightuserdata(lua_State *L, const char *key, void **value, void *default_value);
	void table_get_lightuserdata_not_null(lua_State *L, const char *key, void **value);

	void dispatch_event(lua_State *L, int lua_listener, Event *event);

	void add_task(int listener, Event *event);
	void execute_tasks(lua_State *L);
}

#endif