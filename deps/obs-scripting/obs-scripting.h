#pragma once

#include <util/c99defs.h>

struct obs_script;
typedef struct obs_script obs_script_t;

EXPORT bool obs_scripting_load(void);
EXPORT void obs_scripting_unload(void);

EXPORT obs_script_t *obs_script_create(const char *path);
EXPORT void obs_script_destroy(obs_script_t *script);

EXPORT bool obs_script_loaded(const obs_script_t *script);
EXPORT bool obs_script_reload(obs_script_t *script);
