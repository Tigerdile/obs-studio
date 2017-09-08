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

extern void add_lua_source_functions(lua_State *script);
