#include "obs-scripting-lua.h"
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

/* ========================================================================= */

static void unload_plugin_script(lua_State *script)
{
	void *ud = NULL;
	lua_getallocf(script, &ud);

	struct lua_extra_data *data = ud;
	pthread_mutex_lock(&data->mutex);

	lua_getglobal(script, "obs_module_unload");
	lua_pcall(script, 0, 0, 0);

	pthread_mutex_unlock(&data->mutex);

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

	struct lua_extra_data *data = bmalloc(sizeof(*data));
	pthread_mutexattr_t attr;

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

	lua_getglobal(script, "obs_module_load");

	if (lua_pcall(script, 0, LUA_MULTRET, 0) != 0) {
		warn("Error calling obs_module_Load for '%s': %s",
				file,
				lua_tostring(script, -1));
	}

	ret = lua_gettop(script);
	if (ret == 1 && lua_isboolean(script, -1)) {
		bool success = lua_toboolean(script, -1);

		if (!success) {
			goto fail;
		}
	}

	lua_settop(script, 0);
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

void obs_lua_load(void)
{
	struct dstr dep_paths = {0};
	struct dstr tmp = {0};

	da_init(plugin_scripts);

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
}

void obs_lua_unload(void)
{
	for (size_t i = 0; i < plugin_scripts.num; i++)
		unload_plugin_script(plugin_scripts.array[i]);
	da_free(plugin_scripts);
	bfree(startup_script);
}
