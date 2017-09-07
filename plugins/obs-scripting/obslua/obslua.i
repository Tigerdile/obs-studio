%module obslua
%{
#define SWIG_FILE_WITH_INIT
#define DEPRECATED_START
#define DEPRECATED_END
#include <graphics/graphics.h>
#include <graphics/vec4.h>
#include <graphics/vec3.h>
#include <graphics/vec2.h>
#include <graphics/quat.h>
#include <obs.h>
#include <obs-source.h>
#include <obs-data.h>
#include <obs-properties.h>
#include <obs-interaction.h>
#include <callback/calldata.h>
#include <callback/decl.h>
#include <callback/proc.h>
#include <callback/signal.h>
#include <util/bmem.h>
#include <util/base.h>
#include "cstrcache.h"
%}

#define DEPRECATED_START
#define DEPRECATED_END
#define EXPORT

%rename(blog) wrap_blog;
%inline %{
static inline void wrap_blog(int log_level, const char *message)
{
        blog(log_level, "%s", message);
}
%}

%include "stdint.i"

%ignore blog;
%ignore blogva;
%ignore bcrash;
%ignore obs_source_info;
%ignore obs_register_source_s(const struct obs_source_info *info, size_t size);
%ignore obs_output_set_video(obs_output_t *output, video_t *video);
%ignore obs_output_video(const obs_output_t *output);

%include "graphics/graphics.h"
%include "graphics/vec4.h"
%include "graphics/vec3.h"
%include "graphics/vec2.h"
%include "graphics/quat.h"
%include "obs-data.h"
%include "obs-source.h"
%include "obs-properties.h"
%include "obs-interaction.h"
%include "obs.h"
#include "callback/calldata.h"
#include "callback/decl.h"
#include "callback/proc.h"
#include "callback/signal.h"
%include "util/bmem.h"
%include "util/base.h"
