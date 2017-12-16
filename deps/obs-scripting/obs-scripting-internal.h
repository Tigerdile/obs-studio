#pragma once

#include <util/dstr.h>
#include "obs-scripting.h"

struct obs_script {
	enum obs_script_lang type;
	bool loaded;
	struct dstr path;
};
