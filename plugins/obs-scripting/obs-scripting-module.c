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

/* We use release libraries even when OBS is built in debug mode, if _DEBUG is
 * not undefined on windows it will prevent it from building */

#include <obs-module.h>
#include <util/platform.h>

#include "obs-scripting-config.h"

#if COMPILE_PYTHON
extern void python_load(void);
extern void python_post_maload(void);
extern void python_unload(void);
#endif

#if COMPILE_LUA
extern void obs_lua_load(void);
extern void obs_lua_unload(void);
#endif

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-scripting", "en-US")

bool obs_module_load(void)
{
#if COMPILE_PYTHON
	python_load();
#endif

#if COMPILE_LUA
	obs_lua_load();
#endif

	return true;
}

#if COMPILE_PYTHON
void obs_module_post_load(void)
{
	python_post_maload();
}
#endif

void obs_module_unload()
{
#if COMPILE_LUA
	obs_lua_unload();
#endif

#if COMPILE_PYTHON
	python_unload();
#endif
}
