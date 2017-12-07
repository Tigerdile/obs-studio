#pragma once

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define SWIG_TYPE_TABLE obslua
#include "swig/swigluarun.h"

#include <util/threading.h>
#include <util/base.h>

#define do_log(level, format, ...) \
	blog(level, "[Lua] " format, ##__VA_ARGS__)

#define warn(format, ...)  do_log(LOG_WARNING, format, ##__VA_ARGS__)
#define info(format, ...)  do_log(LOG_INFO,    format, ##__VA_ARGS__)
#define debug(format, ...) do_log(LOG_DEBUG,   format, ##__VA_ARGS__)

struct lua_extra_data {
	pthread_mutex_t mutex;
};

static inline void *lock_script_(lua_State *script)
{
	void *ud = NULL;
	lua_getallocf(script, &ud);

	struct lua_extra_data *data = ud;
	pthread_mutex_lock(&data->mutex);
	return ud;
}

static inline void unlock_script_(void *ud)
{
	struct lua_extra_data *data = ud;
	pthread_mutex_unlock(&data->mutex);
}

#define lock_script(script) \
	void *ud__ = lock_script_(script)
#define unlock_script() \
	unlock_script_(ud__)


extern void add_lua_source_functions(lua_State *script);
