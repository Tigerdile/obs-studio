/********************************************************************************
Copyright (C) 2014 Andrew Skinner <obs@theandyroid.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
********************************************************************************/

#pragma once

#if defined(_WIN32) && defined(_DEBUG)
# undef _DEBUG
# include <Python.h>
# define _DEBUG
#else
# include <Python.h>
#endif

#include <structmember.h>

#define SWIG_TYPE_TABLE obspython
#include "swig/swigpyrun.h"

#ifdef _WIN32
#define __func__ __FUNCTION__
#else
#include <dlfcn.h>
#endif

#include <util/base.h>

#define do_log(level, format, ...) \
	blog(level, "[Python] " format, ##__VA_ARGS__)

#define warn(format, ...)  do_log(LOG_WARNING, format, ##__VA_ARGS__)
#define info(format, ...)  do_log(LOG_INFO,    format, ##__VA_ARGS__)
#define debug(format, ...) do_log(LOG_DEBUG,   format, ##__VA_ARGS__)

/* ------------------------------------------------------------ */

static inline bool py_error_(const char *func, int line)
{
	if (PyErr_Occurred()) {
		warn("Python failure in %s:%d:", func, line);
		PyErr_Print();
		return true;
	}
	return false;
}

#define py_error() py_error_(__FUNCTION__, __LINE__)

#define lock_python() \
	PyGILState_STATE gstate = PyGILState_Ensure()
#define unlock_python() \
	PyGILState_Release(gstate)

struct py_source;
typedef struct py_source py_source_t;

extern PyObject* py_libobs;

extern void py_to_obs_source_info(py_source_t *py_info);
extern PyObject *py_obs_register_source(PyObject *self, PyObject *args);
extern PyObject *py_obs_get_script_config_path(PyObject *self, PyObject *args);

/* ------------------------------------------------------------ */
/* Warning: the following functions expect python to be locked! */

extern bool py_to_libobs_(const char *type,
                          PyObject *  py_in,
                          void *      libobs_out,
                          const char *id,
                          const char *func,
                          int         line);

extern bool libobs_to_py_(const char *type,
                          void *      libobs_in,
                          bool        ownership,
                          PyObject ** py_out,
                          const char *id,
                          const char *func,
                          int         line);

extern bool py_call(PyObject *call, PyObject **ret, const char *arg_def, ...);
extern bool py_import_script(const char *name);
