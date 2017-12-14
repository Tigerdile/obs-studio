#pragma once

#include <util/dstr.h>
#include "obs-scripting.h"

enum obs_script_type {
	OBS_SCRIPT_TYPE_LUA
};

struct obs_script {
	enum obs_script_type type;
	bool loaded;
	bool unloadable;
	struct dstr path;
};
