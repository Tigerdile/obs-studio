#include "obs-scripting-lua.h"
#include "obs-scripting-config.h"
#include <util/platform.h>
#include <util/base.h>
#include <util/dstr.h>

#include <obs.h>

/* ========================================================================= */

#if ARCH_BITS == 64
# define ARCH_DIR "64bit"
#else
# define ARCH_DIR "32bit"
#endif

static const char *startup_script_template = "\
for val in pairs(package.preload) do\n\
	package.preload[val] = nil\n\
end\n\
require \"obslua\"\n\
package.path = package.path .. \"%s\"\n";

static const char *get_script_path_func = "\
function get_script_path()\n\
	 return \"%s\"\n\
end\n";

static char *startup_script = NULL;

static pthread_mutex_t tick_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct obs_lua_script *first_tick_script = NULL;

pthread_mutex_t detach_lua_mutex = PTHREAD_MUTEX_INITIALIZER;
struct lua_obs_callback *detached_lua_callbacks = NULL;

/* ========================================================================= */

static void add_hook_functions(lua_State *script);
static int obs_lua_remove_tick_callback(lua_State *script);
static int obs_lua_remove_main_render_callback(lua_State *script);

#if UI_ENABLED
void add_lua_frontend_funcs(lua_State *script);
#endif

static void *luaalloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
	UNUSED_PARAMETER(ud);
	UNUSED_PARAMETER(osize);

	if (nsize == 0) {
		bfree(ptr);
		return NULL;
	} else {
		return brealloc(ptr, nsize);
	}
}

static bool load_lua_script(struct obs_lua_script *data)
{
	struct dstr str = {0};
	bool success = false;
	int ret;

	lua_State *script = lua_newstate(luaalloc, data);
	if (!script) {
		warn("Failed to create new lua state for '%s'",
				data->base.path.array);
		goto fail;
	}

	pthread_mutex_lock(&data->mutex);

	luaL_openlibs(script);

	if (luaL_dostring(script, startup_script) != 0) {
		warn("Error executing startup script for plugin '%s': %s",
				data->base.path.array,
				lua_tostring(script, -1));
		goto fail;
	}

	dstr_printf(&str, get_script_path_func, data->dir.array);
	ret = luaL_dostring(script, str.array);
	dstr_free(&str);

	if (ret != 0) {
		warn("Error executing startup script for plugin '%s': %s",
				data->base.path.array,
				lua_tostring(script, -1));
		goto fail;
	}

	add_lua_source_functions(script);
	add_hook_functions(script);
#if UI_ENABLED
	add_lua_frontend_funcs(script);
#endif

	if (luaL_loadfile(script, data->file.array) != 0) {
		warn("Error loading plugin '%s': %s",
				data->base.path.array,
				lua_tostring(script, -1));
		goto fail;
	}

	if (lua_pcall(script, 0, LUA_MULTRET, 0) != 0) {
		warn("Error loading plugin '%s': %s",
				data->base.path.array,
				lua_tostring(script, -1));
		goto fail;
	}

	ret = lua_gettop(script);
	if (ret == 1 && lua_isboolean(script, -1)) {
		bool success = lua_toboolean(script, -1);

		if (!success) {
			goto fail;
		}
	}

	lua_getglobal(script, "script_tick");
	if (lua_isfunction(script, -1)) {
		pthread_mutex_lock(&tick_mutex);

		struct obs_lua_script *next = first_tick_script;
		data->next_tick = next;
		data->p_prev_next_tick = &first_tick_script;
		if (next) next->p_prev_next_tick = &data->next_tick;
		first_tick_script = data;

		data->tick = luaL_ref(script, LUA_REGISTRYINDEX);

		pthread_mutex_unlock(&tick_mutex);
	}

	data->script = script;
	success = true;

fail:
	if (script) {
		lua_settop(script, 0);
		pthread_mutex_unlock(&data->mutex);
	}

	if (!success) {
		lua_close(script);
	}

	return success;
}

#define ls_get_libobs_obj(type, lua_index, obs_obj) \
	ls_get_libobs_obj_(script, #type " *", lua_index, obs_obj, \
			NULL, __FUNCTION__, __LINE__)
#define ls_push_libobs_obj(type, obs_obj, ownership) \
	ls_push_libobs_obj_(script, #type " *", obs_obj, ownership, \
			NULL, __FUNCTION__, __LINE__)
#define call_func(name, args, rets) \
	call_func_(script, cb->reg_idx, \
			args, rets, #name, __FUNCTION__)

/* -------------------------------------------- */

static void obs_lua_main_render_callback(void *priv, uint32_t cx, uint32_t cy)
{
	struct lua_obs_callback *cb = priv;
	lua_State *script = cb->script;

	if (cb->remove)
		obs_remove_main_render_callback(obs_lua_main_render_callback,
				cb);

	lock_script(script);

	if (cb->remove) {
		free_lua_obs_callback(cb);
	} else {
		lua_pushinteger(script, (lua_Integer)cx);
		lua_pushinteger(script, (lua_Integer)cy);
		call_func(obs_lua_main_render_callback, 2, 0);
	}

	unlock_script();
}

static int obs_lua_remove_main_render_callback(lua_State *script)
{
	if (!verify_args1(script, is_function))
		return 0;

	struct lua_obs_callback *cb = find_lua_obs_callback(script, 1);
	if (cb) remove_lua_obs_callback(cb);
	return 0;
}

static int obs_lua_add_main_render_callback(lua_State *script)
{
	if (!verify_args1(script, is_function))
		return 0;

	struct lua_obs_callback *cb = add_lua_obs_callback(script, 1);
	obs_add_main_render_callback(obs_lua_main_render_callback, cb);
	return 0;
}

/* -------------------------------------------- */

static THREAD_LOCAL struct lua_obs_callback *signal_current_cb = NULL;

/* -------------------------------------------- */

static void calldata_signal_callback(void *priv, calldata_t *cd)
{
	struct lua_obs_callback *cb = priv;
	struct lua_obs_callback *last_current = signal_current_cb;
	lua_State *script = cb->script;
	bool call_again = false;

	if (cb->remove)
		signal_handler_remove_current();
	else
		signal_current_cb = cb;

	lock_script(script);

	if (cb->remove) {
		free_lua_obs_callback(cb);
	} else {
		ls_push_libobs_obj(calldata_t, cd, false);
		call_func(calldata_signal_callback, 1, 0);

		signal_current_cb = last_current;

		if (cb->remove)
			call_again = true;
	}

	unlock_script();

	if (call_again)
		calldata_signal_callback(priv, cd);
}

static int obs_lua_signal_handler_disconnect(lua_State *script)
{
	signal_handler_t *handler;
	const char *signal;

	if (!ls_get_libobs_obj(signal_handler_t, 1, &handler))
		return 0;
	signal = lua_tostring(script, 2);
	if (!signal)
		return 0;
	if (!is_function(script, 3))
		return 0;

	struct lua_obs_callback *cb = find_lua_obs_callback(script, 3);
	while (cb) {
		signal_handler_t *cb_handler =
			calldata_ptr(&cb->extra, "handler");
		const char *cb_signal =
			calldata_string(&cb->extra, "signal");

		if (cb_signal &&
		    strcmp(signal, cb_signal) != 0 &&
		    handler == cb_handler)
			break;

		cb = find_next_lua_obs_callback(script, cb, 3);
	}

	if (cb) remove_lua_obs_callback(cb);
	return 0;
}

static int obs_lua_signal_handler_connect(lua_State *script)
{
	signal_handler_t *handler;
	const char *signal;

	if (!ls_get_libobs_obj(signal_handler_t, 1, &handler))
		return 0;
	signal = lua_tostring(script, 2);
	if (!signal)
		return 0;
	if (!is_function(script, 3))
		return 0;

	struct lua_obs_callback *cb = add_lua_obs_callback(script, 3);
	calldata_set_ptr(&cb->extra, "handler", handler);
	calldata_set_string(&cb->extra, "signal", signal);
	signal_handler_connect(handler, signal, calldata_signal_callback, cb);
	return 0;
}

/* -------------------------------------------- */

static void calldata_signal_callback_global(void *priv, const char *signal,
		calldata_t *cd)
{
	struct lua_obs_callback *cb = priv;
	struct lua_obs_callback *last_current = signal_current_cb;
	lua_State *script = cb->script;
	bool call_again = false;

	if (cb->remove)
		signal_handler_remove_current();
	else
		signal_current_cb = cb;

	lock_script(script);

	if (cb->remove) {
		remove_lua_obs_callback(cb);
	} else {
		lua_pushstring(script, signal);
		ls_push_libobs_obj(calldata_t, cd, false);
		call_func(calldata_signal_callback_global, 2, 0);

		signal_current_cb = last_current;

		if (cb->remove)
			call_again = true;
	}

	unlock_script();

	if (call_again)
		calldata_signal_callback_global(priv, signal, cd);
}

static int obs_lua_signal_handler_disconnect_global(lua_State *script)
{
	if (!verify_args1(script, is_function))
		return 0;

	struct lua_obs_callback *cb = find_lua_obs_callback(script, 1);
	if (cb) remove_lua_obs_callback(cb);
	return 0;
}

static int obs_lua_signal_handler_connect_global(lua_State *script)
{
	signal_handler_t *handler;

	if (!ls_get_libobs_obj(signal_handler_t, 1, &handler))
		return 0;
	if (!is_function(script, 2))
		return 0;

	struct lua_obs_callback *cb = add_lua_obs_callback(script, 2);
	signal_handler_connect_global(handler,
			calldata_signal_callback_global, cb);
	return 0;
}

/* -------------------------------------------- */

static int obs_lua_signal_handler_remove_current(lua_State *script)
{
	UNUSED_PARAMETER(script);
	if (signal_current_cb)
		remove_lua_obs_callback(signal_current_cb);
	return 0;
}

/* -------------------------------------------- */

static int calldata_source(lua_State *script)
{
	calldata_t *cd;
	const char *str;
	int rets = 0;

	if (!ls_get_libobs_obj(calldata_t, 1, &cd))
		goto fail;
	str = lua_tostring(script, 2);
	if (!str)
		goto fail;

	obs_source_t *source = calldata_ptr(cd, str);
	if (ls_push_libobs_obj(obs_source_t, source, false))
		++rets;

fail:
	return rets;
}

/* -------------------------------------------- */

static void add_hook_functions(lua_State *script)
{
	lua_getglobal(script, "obslua");

	lua_pushstring(script, "obs_add_main_render_callback");
	lua_pushcfunction(script, obs_lua_add_main_render_callback);
	lua_rawset(script, -3);

	lua_pushstring(script, "obs_remove_main_render_callback");
	lua_pushcfunction(script, obs_lua_remove_main_render_callback);
	lua_rawset(script, -3);

	lua_pushstring(script, "calldata_source");
	lua_pushcfunction(script, calldata_source);
	lua_rawset(script, -3);

	lua_pushstring(script, "signal_handler_connect");
	lua_pushcfunction(script, obs_lua_signal_handler_connect);
	lua_rawset(script, -3);

	lua_pushstring(script, "signal_handler_disconnect");
	lua_pushcfunction(script, obs_lua_signal_handler_disconnect);
	lua_rawset(script, -3);

	lua_pushstring(script, "signal_handler_connect_global");
	lua_pushcfunction(script, obs_lua_signal_handler_connect_global);
	lua_rawset(script, -3);

	lua_pushstring(script, "signal_handler_disconnect_global");
	lua_pushcfunction(script, obs_lua_signal_handler_disconnect_global);
	lua_rawset(script, -3);

	lua_pushstring(script, "signal_handler_remove_current");
	lua_pushcfunction(script, obs_lua_signal_handler_remove_current);
	lua_rawset(script, -3);

	lua_pop(script, 1);
}

/* -------------------------------------------- */

static void lua_tick(void *param, float seconds)
{
	struct obs_lua_script *data;

	pthread_mutex_lock(&tick_mutex);
	data = first_tick_script;
	while (data) {
		lua_State *script = data->script;

		pthread_mutex_lock(&data->mutex);

		lua_pushnumber(script, (double)seconds);
		call_func_(script, data->tick, 1, 0, "tick", __FUNCTION__);

		pthread_mutex_unlock(&data->mutex);

		data = data->next_tick;
	}
	pthread_mutex_unlock(&tick_mutex);

	UNUSED_PARAMETER(param);
}

/* -------------------------------------------- */

bool obs_lua_script_load(obs_script_t *s)
{
	struct obs_lua_script *data = (struct obs_lua_script *)s;
	if (!data->base.loaded) {
		data->base.loaded = load_lua_script(data);
	}

	return data->base.loaded;
}

obs_script_t *obs_lua_script_create(const char *path)
{
	struct obs_lua_script *data = bzalloc(sizeof(*data));

	data->base.type = OBS_SCRIPT_TYPE_LUA;
	data->tick = LUA_REFNIL;

	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutex_init_value(&data->mutex);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

	if (pthread_mutex_init(&data->mutex, &attr) != 0) {
		bfree(data);
		return NULL;
	}

	dstr_copy(&data->base.path, path);

	char *slash = path && *path ? strrchr(path, '/') : NULL;
	if (slash) {
		slash++;
		dstr_copy(&data->file, slash);
		dstr_left(&data->dir, &data->base.path, slash - path);
	} else {
		dstr_copy(&data->file, path);
	}

	dstr_copy(&data->base.path, path);

	obs_lua_script_load((obs_script_t *)data);
	return (obs_script_t *)data;
}

void obs_lua_script_unload(obs_script_t *s)
{
	struct obs_lua_script *data = (struct obs_lua_script *)s;

	if (!s->loaded)
		return;

	lua_State *script = data->script;
	
	/* ---------------------------- */
	/* unhook tick function         */

	if (data->p_prev_next_tick) {
		pthread_mutex_lock(&tick_mutex);

		struct obs_lua_script *next = data->next_tick;
		if (next) next->p_prev_next_tick = data->p_prev_next_tick;
		*data->p_prev_next_tick = next;

		pthread_mutex_unlock(&tick_mutex);

		data->p_prev_next_tick = NULL;
		data->next_tick = NULL;
	}

	/* ---------------------------- */
	/* call script_unload           */

	pthread_mutex_lock(&data->mutex);

	lua_getglobal(script, "script_unload");
	lua_pcall(script, 0, 0, 0);

	struct lua_obs_callback *cb = data->first_callback;
	while (cb) {
		struct lua_obs_callback *next = cb->next;
		remove_lua_obs_callback(cb);
		cb = next;
	}

	pthread_mutex_unlock(&data->mutex);

	/* ---------------------------- */
	/* close script                 */

	lua_close(script);
	s->loaded = false;
}

void obs_lua_script_destroy(obs_script_t *s)
{
	struct obs_lua_script *data = (struct obs_lua_script *)s;

	if (data) {
		pthread_mutex_destroy(&data->mutex);
		bfree(data);
	}
}

/* -------------------------------------------- */

void obs_lua_load(void)
{
	struct dstr dep_paths = {0};
	struct dstr tmp = {0};

	pthread_mutex_init(&tick_mutex, NULL);
	pthread_mutex_init(&detach_lua_mutex, NULL);

	/* ---------------------------------------------- */
	/* Initialize Lua plugin dep script paths         */

	dstr_copy(&dep_paths, "");

	const char **path = obs_get_parsed_search_paths("dep_scripts");
	while (*path) {
		if (dep_paths.len)
			dstr_cat(&dep_paths, ";");
		dstr_cat(&dep_paths, *path);
		dstr_cat(&dep_paths, "/?.lua");
		++path;
	}

	/* ---------------------------------------------- */
	/* Initialize Lua startup script                  */

	dstr_printf(&tmp, startup_script_template,
			dep_paths.array);
	startup_script = tmp.array;

	dstr_free(&dep_paths);

	obs_add_tick_callback(lua_tick, NULL);
}

void obs_lua_unload(void)
{
	pthread_mutex_lock(&detach_lua_mutex);

	struct lua_obs_callback *cur = detached_lua_callbacks;
	while (cur) {
		struct lua_obs_callback *next = cur->next;
		just_free_lua_obs_callback(next);
		cur = next;
	}

	pthread_mutex_unlock(&detach_lua_mutex);

	/* ---------------------- */

	bfree(startup_script);
	pthread_mutex_destroy(&tick_mutex);
	pthread_mutex_destroy(&detach_lua_mutex);
}
