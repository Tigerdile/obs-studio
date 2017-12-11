#pragma once

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define SWIG_TYPE_TABLE obslua
#include "swig/swigluarun.h"

#include <callback/calldata.h>
#include <util/threading.h>
#include <util/base.h>
#include <util/bmem.h>

#define do_log(level, format, ...) \
	blog(level, "[Lua] " format, ##__VA_ARGS__)

#define warn(format, ...)  do_log(LOG_WARNING, format, ##__VA_ARGS__)
#define info(format, ...)  do_log(LOG_INFO,    format, ##__VA_ARGS__)
#define debug(format, ...) do_log(LOG_DEBUG,   format, ##__VA_ARGS__)

struct lua_obs_callback;

struct lua_extra_data {
	pthread_mutex_t mutex;
	struct lua_obs_callback *first_callback;
	lua_State *script;

	int tick;
	struct lua_extra_data *next_tick;
	struct lua_extra_data **p_prev_next_tick;
};

static inline struct lua_extra_data *lock_script_(lua_State *script)
{
	void *ud = NULL;
	lua_getallocf(script, &ud);

	struct lua_extra_data *data = ud;
	pthread_mutex_lock(&data->mutex);
	return data;
}

static inline void unlock_script_(struct lua_extra_data *data)
{
	pthread_mutex_unlock(&data->mutex);
}

#define lock_script(script) \
	struct lua_extra_data *ud__ = lock_script_(script)
#define unlock_script() \
	unlock_script_(ud__)

/* ------------------------------------------------ */

struct lua_obs_callback {
	struct lua_obs_callback *next;
	struct lua_obs_callback **p_prev_next;

	lua_State *script;
	int reg_idx;
	bool remove;
	calldata_t extra;
};

static inline struct lua_obs_callback *add_lua_obs_callback(
		lua_State *script, int stack_idx)
{
	struct lua_obs_callback *cb = bzalloc(sizeof(*cb));

	void *ud = NULL;
	lua_getallocf(script, &ud);
	struct lua_extra_data *data = ud;

	struct lua_obs_callback *next = data->first_callback;
	cb->next = next;
	cb->p_prev_next = &data->first_callback;
	if (next) next->p_prev_next = &cb->next;

	lua_pushvalue(script, stack_idx);
	cb->reg_idx = luaL_ref(script, LUA_REGISTRYINDEX);
	cb->script = script;
	cb->remove = false;

	data->first_callback = cb;
	return cb;
}

static inline struct lua_obs_callback *find_next_lua_obs_callback(
		lua_State *script, struct lua_obs_callback *cb, int stack_idx)
{
	void *ud = NULL;
	lua_getallocf(script, &ud);
	struct lua_extra_data *data = ud;

	cb = cb ? cb->next : data->first_callback;

	while (cb) {
		lua_rawgeti(script, LUA_REGISTRYINDEX, cb->reg_idx);
		bool match = lua_rawequal(script, -1, stack_idx);
		lua_pop(script, 1);

		if (match)
			break;

		cb = cb->next;
	}

	return cb;
}

static inline struct lua_obs_callback *find_lua_obs_callback(
		lua_State *script, int stack_idx)
{
	return find_next_lua_obs_callback(script, NULL, stack_idx);
}

static inline void remove_lua_obs_callback(struct lua_obs_callback *cb)
{
	luaL_unref(cb->script, LUA_REGISTRYINDEX, cb->reg_idx);
	calldata_free(&cb->extra);

	struct lua_obs_callback *next = cb->next;
	if (next) next->p_prev_next = cb->p_prev_next;
	*cb->p_prev_next = cb->next;
	bfree(cb);
}

/* ------------------------------------------------ */

static int is_ptr(lua_State *script, int idx)
{
	return lua_isuserdata(script, idx) || lua_isnil(script, idx);
}

static int is_table(lua_State *script, int idx)
{
	return lua_istable(script, idx);
}

static int is_function(lua_State *script, int idx)
{
	return lua_isfunction(script, idx);
}

typedef int (*param_cb)(lua_State *script, int idx);

static inline bool verify_args1_(lua_State *script,
		param_cb param1_check,
		const char *func)
{
	if (lua_gettop(script) != 1) {
		warn("Wrong number of parameters for %s", func);
		return false;
	}
	if (!param1_check(script, 1)) {
		warn("Wrong parameter type for parameter %d of %s", 1, func);
		return false;
	}

	return true;
}

#define verify_args1(script, param1_check) \
	verify_args1_(script, param1_check, __FUNCTION__)

static inline bool call_func_(lua_State *script,
		int reg_idx, int args, int rets,
		const char *func, const char *display_name)
{
	if (reg_idx == LUA_REFNIL)
		return false;

	lua_rawgeti(script, LUA_REGISTRYINDEX, reg_idx);
	lua_insert(script, -1 - args);

	if (lua_pcall(script, args, rets, 0) != 0) {
		warn("Failed to call %s for %s: %s", func, display_name,
				lua_tostring(script, -1));
		lua_pop(script, 1);
		return false;
	}

	return true;
}

bool ls_get_libobs_obj_(lua_State * script,
                        const char *type,
                        int         lua_idx,
                        void *      libobs_out,
                        const char *id,
                        const char *func,
                        int         line);
bool ls_push_libobs_obj_(lua_State * script,
                         const char *type,
                         void *      libobs_in,
                         bool        ownership,
                         const char *id,
                         const char *func,
                         int         line);

extern void add_lua_source_functions(lua_State *script);
