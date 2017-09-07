%module(threads="1") obspython
%nothread;
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
#include "../cstrcache.h"
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

%typemap(in) uint8_t *data {
	if(!PyByteArray_Check($input)) {
		SWIG_exception_fail(SWIG_TypeError, "Expected a bytearray");
	}
	$1 = PyByteArray_AsString($input);
}


%typemap(in) uint8_t **data (uint8_t *tmp)
{
	if(!PyByteArray_Check($input)) {
		SWIG_exception_fail(SWIG_TypeError, "Expected a bytearray");
	}
	tmp  = PyByteArray_AsString($input);
	$1 = &tmp;
}

/*buf and alloc are expected*/
%typemap(in) const char *(char *buf, PyObject *py_bytes,
                          int alloc, const char *cache_val)
{
	alloc = SWIG_OLDOBJ; //Do not free 'buf'

	/* Python 3 Only */
	if (!PyUnicode_Check($input)) {
		SWIG_exception_fail(SWIG_TypeError, "Expected a string");
	}

	py_bytes = PyUnicode_AsUTF8String($input);
	buf = PyBytes_AsString(py_bytes);
	$1 = cstrcache_get(buf);
        Py_XDECREF(py_bytes);
}

/* Used to free when using %newobject functions.  E.G.:
 * %newobject obs_module_get_config_path; */
%typemap(newfree) char * "bfree($1);";

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

/* declare these manually because mutex + GIL = deadlocks */
%thread;
void obs_enter_graphics(void); //Should only block on entering mutex
%nothread;
%include "obs.h"
