/******************************************************************************
    Copyright (C) 2015 by Andrew Skinner <obs@theandyroid.com>
    Copyright (C) 2017 by Hugh Bailey <jim@obsproject.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include <obs.h>
#include <util/dstr.h>
#include <util/platform.h>

#include "obs-scripting-internal.h"
#include "obs-scripting-config.h"

#if COMPILE_LUA
extern obs_script_t *obs_lua_script_create(const char *path);
extern bool obs_lua_script_load(obs_script_t *s);
extern void obs_lua_script_unload(obs_script_t *s);
extern void obs_lua_script_destroy(obs_script_t *s);
extern void obs_lua_load(void);
extern void obs_lua_unload(void);
#endif

static struct dstr file_filter = {0};

static const char *supported_formats[] = {
#ifdef COMPILE_LUA
	"lua",
#endif
	NULL
};

bool obs_scripting_load(void)
{
#if COMPILE_LUA
	obs_lua_load();
#endif

	return true;
}

void obs_scripting_unload(void)
{
#if COMPILE_LUA
	obs_lua_unload();
#endif
	dstr_free(&file_filter);
}

const char **obs_scripting_supported_formats(void)
{
	return supported_formats;
}

static inline bool pointer_valid(const void *x, const char *name,
		const char *func)
{
	if (!x) {
		blog(LOG_WARNING, "obs-scripting: [%s] %s is null",
				func, name);
		return false;
	}

	return true;
}

#define ptr_valid(x) pointer_valid(x, #x, __FUNCTION__)

obs_script_t *obs_script_create(const char *path)
{
	obs_script_t *script = NULL;
	const char *ext;

	if (!ptr_valid(path))
		return NULL;

	ext = strrchr(path, '.');
	if (!ext)
		return NULL;

#if COMPILE_LUA
	if (strcmp(ext, ".lua") == 0) {
		script = obs_lua_script_create(path);
	} else {
		blog(LOG_WARNING, "Unknown script type: %s", path);
	}
#endif

	return script;
}

const char *obs_script_get_path(const obs_script_t *script)
{
	return ptr_valid(script) ? script->path.array : "";
}

bool obs_script_reload(obs_script_t *script)
{
	if (!ptr_valid(script))
		return false;

	if (script->type == OBS_SCRIPT_LANG_LUA) {
#if COMPILE_LUA
		obs_lua_script_unload(script);
		obs_lua_script_load(script);
#endif
	}

	return script->loaded;
}

bool obs_script_loaded(const obs_script_t *script)
{
	return ptr_valid(script) ? script->loaded : false;
}

void obs_script_destroy(obs_script_t *script)
{
	if (!script)
		return;

	if (script->type == OBS_SCRIPT_LANG_LUA) {
#if COMPILE_LUA
		obs_lua_script_unload(script);
		obs_lua_script_destroy(script);
#endif
	}
}
