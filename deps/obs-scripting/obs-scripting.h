#pragma once

#include <util/c99defs.h>

#ifdef __cplusplus
extern "C" {
#endif

struct obs_script;
typedef struct obs_script obs_script_t;

enum obs_script_lang {
	OBS_SCRIPT_LANG_LUA
};

EXPORT bool obs_scripting_load(void);
EXPORT void obs_scripting_unload(void);
EXPORT const char **obs_scripting_supported_formats(void);

EXPORT obs_script_t *obs_script_create(const char *path);
EXPORT void obs_script_destroy(obs_script_t *script);

EXPORT const char *obs_script_get_path(const obs_script_t *script);

EXPORT bool obs_script_loaded(const obs_script_t *script);
EXPORT bool obs_script_reload(obs_script_t *script);

#ifdef __cplusplus
}
#endif
