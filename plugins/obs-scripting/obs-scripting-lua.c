#include "obs-scripting-lua.h"
#include "obs-scripting-config.h"
#include <util/platform.h>
#include <util/base.h>
#include <util/dstr.h>

#include <obs-module.h>

/* ========================================================================= */

#if _WIN32
# define SO_EXT "dll"
#else
# define SO_EXT "so"
#endif

#if ARCH_BITS == 64
# define ARCH_DIR "64bit"
#else
# define ARCH_DIR "32bit"
#endif

static const char *startup_script_template = "\
for val in pairs(package.preload) do\n\
	package.preload[val] = nil\n\
end\n\
local old_cpaths = package.cpath\n\
package.cpath = \"%s/?." SO_EXT "\"\n\
require \"obslua\"\n\
package.cpath = old_cpaths\n\
package.path = package.path .. \"%s\"\n";

static const char *get_script_path_func = "\
function get_script_path()\n\
	 return \"%s\"\n\
end\n";

static DARRAY(lua_State*) plugin_scripts;
static char *startup_script = NULL;

static pthread_mutex_t tick_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct lua_extra_data *first_tick_script = NULL;

/* ========================================================================= */

static void add_hook_functions(lua_State *script);
static int obs_lua_remove_tick_callback(lua_State *script);
static int obs_lua_remove_main_render_callback(lua_State *script);

#if UI_FOUND
void add_lua_frontend_funcs(lua_State *script);
#endif

static void unload_plugin_script(lua_State *script)
{
	void *ud = NULL;
	lua_getallocf(script, &ud);

	struct lua_extra_data *data = ud;
	pthread_mutex_lock(&data->mutex);

	struct lua_obs_callback *cb = data->first_callback;
	while (cb) {
		struct lua_obs_callback *next = cb->next;
		calldata_free(&cb->extra);
		bfree(cb);
		cb = next;
	}

	lua_getglobal(script, "script_unload");
	lua_pcall(script, 0, 0, 0);

	pthread_mutex_unlock(&data->mutex);

	if (data->p_prev_next_tick) {
		pthread_mutex_lock(&tick_mutex);

		struct lua_extra_data *next = data->next_tick;
		if (next) next->p_prev_next_tick = data->p_prev_next_tick;
		*data->p_prev_next_tick = next;

		pthread_mutex_unlock(&tick_mutex);
	}

	lua_close(script);

	if (ud) {
		pthread_mutex_destroy(&data->mutex);
		bfree(data);
	}
}

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

static void load_plugin_script(const char *file, const char *dir)
{
	struct dstr str = {0};
	int ret;

	struct lua_extra_data *data = bzalloc(sizeof(*data));

	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutex_init_value(&data->mutex);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

	if (pthread_mutex_init(&data->mutex, &attr) != 0) {
		goto fail2;
	}

	lua_State *script = lua_newstate(luaalloc, data);
	if (!script) {
		warn("Failed to create new lua state for '%s'", file);
		goto fail2;
	}

	pthread_mutex_lock(&data->mutex);

	luaL_openlibs(script);

	if (luaL_dostring(script, startup_script) != 0) {
		warn("Error executing startup script for plugin '%s': %s",
				file,
				lua_tostring(script, -1));
		goto fail;
	}

	dstr_printf(&str, get_script_path_func, dir);
	ret = luaL_dostring(script, str.array);
	dstr_free(&str);

	if (ret != 0) {
		warn("Error executing startup script for plugin '%s': %s",
				file,
				lua_tostring(script, -1));
		goto fail;
	}

	add_lua_source_functions(script);
	add_hook_functions(script);
#if UI_FOUND
	add_lua_frontend_funcs(script);
#endif

	if (luaL_loadfile(script, file) != 0) {
		warn("Error loading plugin '%s': %s",
				file,
				lua_tostring(script, -1));
		goto fail;
	}

	if (lua_pcall(script, 0, LUA_MULTRET, 0) != 0) {
		warn("Error loading plugin '%s': %s",
				file,
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

		struct lua_extra_data *next = first_tick_script;
		data->next_tick = next;
		data->p_prev_next_tick = &first_tick_script;
		if (next) next->p_prev_next_tick = &data->next_tick;
		first_tick_script = data;

		data->tick = luaL_ref(script, LUA_REGISTRYINDEX);

		pthread_mutex_unlock(&tick_mutex);
	}

	lua_settop(script, 0);
	data->script = script;
	pthread_mutex_unlock(&data->mutex);

	da_push_back(plugin_scripts, &script);
	return;

fail:
	lua_settop(script, 0);
	pthread_mutex_unlock(&data->mutex);

	unload_plugin_script(script);
	return;

fail2:
	pthread_mutex_destroy(&data->mutex);
	bfree(data);
}

static void load_plugin_scripts(const char *path)
{
	struct dstr script_search_path = {0};
	struct os_glob_info *glob;

	dstr_printf(&script_search_path, "%s/*.lua", path);

	if (os_glob(script_search_path.array, 0, &glob) == 0) {
		for (size_t i = 0; i < glob->gl_pathc; i++) {
			const char *file  = glob->gl_pathv[i].path;
			const char *slash = strrchr(file, '/');
			struct dstr dir   = {0};

			dstr_copy(&dir, file);
			dstr_resize(&dir, slash ? slash + 1 - file : 0);

			info("Loading Lua plugin '%s'", file);
			load_plugin_script(file, dir.array);

			dstr_free(&dir);
		}
		os_globfree(glob);
	}

	dstr_free(&script_search_path);
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
		obs_remove_main_render_callback(obs_lua_main_render_callback, cb);

	lock_script(script);

	if (cb->remove) {
		remove_lua_obs_callback(cb);
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
	if (cb) cb->remove = true;
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
		remove_lua_obs_callback(cb);
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

	if (cb) cb->remove = true;

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
	if (cb) cb->remove = true;
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
		signal_current_cb->remove = true;
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
	struct lua_extra_data *data;

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

void obs_lua_load(void)
{
	struct dstr dep_paths = {0};
	struct dstr tmp = {0};

	da_init(plugin_scripts);

	pthread_mutex_init(&tick_mutex, NULL);

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

	char *module_path = obs_module_file(ARCH_DIR);
	if (!module_path)
		module_path = bstrdup("");

	dstr_printf(&tmp, startup_script_template,
			module_path,
			dep_paths.array);
	startup_script = tmp.array;

	bfree(module_path);
	dstr_free(&dep_paths);

	/* ---------------------------------------------- */
	/* Load Lua plugins                               */

	char *builtin_script_path = obs_module_file("scripts");
	if (builtin_script_path) {
		load_plugin_scripts(builtin_script_path);
		bfree(builtin_script_path);
	}

	const char **paths = obs_get_parsed_search_paths("plugin_scripts");
	while (*paths) {
		load_plugin_scripts(*paths);
		paths++;
	}

	obs_add_tick_callback(lua_tick, NULL);
}

void obs_lua_unload(void)
{
	for (size_t i = 0; i < plugin_scripts.num; i++)
		unload_plugin_script(plugin_scripts.array[i]);
	da_free(plugin_scripts);
	bfree(startup_script);
	pthread_mutex_destroy(&tick_mutex);
}
