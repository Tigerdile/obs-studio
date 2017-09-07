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

#pragma once

#include <obs-module.h>
#include "obs-scripting-python.h"
#include "cstrcache.h"

/* ========================================================================= */

/* Object Data */
struct py_source {
	PyObject_HEAD PyObject *id;
	PyObject *              type;
	PyObject *              flags;
	PyObject *              get_name;
	PyObject *              create;
	PyObject *              destroy;
	PyObject *              get_width;
	PyObject *              get_height;
	PyObject *              get_defaults;
	PyObject *              get_properties;
	PyObject *              update;
	PyObject *              activate;
	PyObject *              deactivate;
	PyObject *              show;
	PyObject *              hide;
	PyObject *              video_tick;
	PyObject *              video_render;
	PyObject *              filter_video;
	PyObject *              filter_audio;
	PyObject *              save;
	PyObject *              load;
	PyObject *              mouse_click;
	PyObject *              mouse_move;
	PyObject *              mouse_wheel;
	PyObject *              focus;
	PyObject *              key_click;
	PyObject *              data;

	/* C types below this point */
	struct obs_source_info  py_source_info;
	const char *            name;
};

struct python_source {
	py_source_t *         source;
	struct python_source *next;
	struct python_source *prev;
};

struct python_data_pair {
	py_source_t *source;
	PyObject *   data;
};

static void py_source_dealloc(py_source_t *self)
{
	Py_XDECREF(self->id);
	Py_XDECREF(self->data);
	Py_XDECREF(self->type);
	Py_XDECREF(self->flags);
	Py_XDECREF(self->get_name);
	Py_XDECREF(self->create);
	Py_XDECREF(self->destroy);
	Py_XDECREF(self->get_width);
	Py_XDECREF(self->get_height);
	Py_XDECREF(self->get_defaults);
	Py_XDECREF(self->get_properties);
	Py_XDECREF(self->update);
	Py_XDECREF(self->activate);
	Py_XDECREF(self->deactivate);
	Py_XDECREF(self->show);
	Py_XDECREF(self->hide);
	Py_XDECREF(self->video_tick);
	Py_XDECREF(self->video_render);
	Py_XDECREF(self->filter_video);
	Py_XDECREF(self->filter_audio);
	Py_XDECREF(self->save);
	Py_XDECREF(self->load);
	Py_XDECREF(self->mouse_click);
	Py_XDECREF(self->mouse_move);
	Py_XDECREF(self->mouse_wheel);
	Py_XDECREF(self->focus);
	Py_XDECREF(self->key_click);

	Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject * py_source_new(PyTypeObject *type, PyObject *args,
		PyObject *kwds)
{
	UNUSED_PARAMETER(args);
	UNUSED_PARAMETER(kwds);
	py_source_t *self;

	self = (py_source_t *)type->tp_alloc(type, 0);

	if (self != NULL) {
		self->id = PyUnicode_FromString("python_default");
		if (self->id == NULL) {
			Py_DECREF(self);
			return NULL;
		}
		self->type = PyLong_FromLong(OBS_SOURCE_TYPE_INPUT);
		if (self->type == NULL) {
			Py_DECREF(self);
			return NULL;
		}
		self->flags = PyLong_FromLong(OBS_SOURCE_VIDEO);
		if (self->flags == NULL) {
			Py_DECREF(self);
			return NULL;
		}
#define ADD_MEMBER(x) \
		do { \
			self->x = Py_BuildValue(""); \
			if (!self->x) { \
				Py_DECREF(self); \
				return NULL; \
			} \
		} \
		while (false)

		ADD_MEMBER(get_name);
		ADD_MEMBER(create);
		ADD_MEMBER(destroy);
		ADD_MEMBER(data);
		ADD_MEMBER(get_width);
		ADD_MEMBER(get_height);
		ADD_MEMBER(get_defaults);
		ADD_MEMBER(get_properties);
		ADD_MEMBER(update);
		ADD_MEMBER(activate);
		ADD_MEMBER(deactivate);
		ADD_MEMBER(show);
		ADD_MEMBER(hide);
		ADD_MEMBER(video_tick);
		ADD_MEMBER(video_render);
		ADD_MEMBER(filter_video);
		ADD_MEMBER(filter_audio);
		ADD_MEMBER(save);
		ADD_MEMBER(load);
		ADD_MEMBER(mouse_click);
		ADD_MEMBER(mouse_move);
		ADD_MEMBER(mouse_wheel);
		ADD_MEMBER(focus);
		ADD_MEMBER(key_click);
#undef ADD_MEMBER
	}

	self->name = cstrcache_get("Default Python Script");
	memset(&self->py_source_info, 0, sizeof(self->py_source_info));;
	return (PyObject *)self;
}

static int py_source_init(py_source_t *self, PyObject *args, PyObject *kwds)
{
	UNUSED_PARAMETER(self);
	UNUSED_PARAMETER(args);
	UNUSED_PARAMETER(kwds);
	/*Do nothing for now*/
	return 0;
}

/*Method Table*/
static PyMethodDef py_source_methods[] = {{0}};

#define PYTABLE_ITEM(name) \
	{#name, T_OBJECT_EX, offsetof(py_source_t, name), 0, #name}

/*Member table*/
static PyMemberDef py_source_members[] = {
	PYTABLE_ITEM(id),
	PYTABLE_ITEM(type),
	PYTABLE_ITEM(flags),
	PYTABLE_ITEM(create),
	PYTABLE_ITEM(destroy),
	PYTABLE_ITEM(get_name),
	PYTABLE_ITEM(get_width),
	PYTABLE_ITEM(get_height),
	PYTABLE_ITEM(get_defaults),
	PYTABLE_ITEM(get_properties),
	PYTABLE_ITEM(update),
	PYTABLE_ITEM(activate),
	PYTABLE_ITEM(deactivate),
	PYTABLE_ITEM(show),
	PYTABLE_ITEM(hide),
	PYTABLE_ITEM(video_tick),
	PYTABLE_ITEM(video_render),
	PYTABLE_ITEM(filter_video),
	PYTABLE_ITEM(filter_audio),
	PYTABLE_ITEM(save),
	PYTABLE_ITEM(load),
	PYTABLE_ITEM(mouse_click),
	PYTABLE_ITEM(mouse_move),
	PYTABLE_ITEM(mouse_wheel),
	PYTABLE_ITEM(focus),
	PYTABLE_ITEM(key_click),
        {0}
};

#undef PYTABLE_ITEM

/* ========================================================================= */

#define func_callable(func) PyCallable_Check(py_src->func)

bool py_to_libobs_(const char *type,
                   PyObject *  py_in,
                   void *      libobs_out,
                   const char *id,
                   const char *func,
                   int         line)
{
	swig_type_info *info = SWIG_TypeQuery(type);
	if (info == NULL) {
		warn("%s:%d: SWIG could not find type: %s%s%s",
		     func,
		     line,
		     id ? id : "",
		     id ? "::" : "",
		     type);
		return false;
	}

	int ret = SWIG_ConvertPtr(py_in, libobs_out, info, 0);
	if (!SWIG_IsOK(ret)) {
		warn("%s:%d: SWIG failed to convert python object to obs "
		     "object: %s%s%s",
		     func,
		     line,
		     id ? id : "",
		     id ? "::" : "",
		     type);
		return false;
	}

	return true;
}

#define py_to_libobs(type, py_obj, obs_obj) \
	py_to_libobs_(#type " *", py_obj, obs_obj, \
			py_src->py_source_info.id, __func__, __LINE__)

bool libobs_to_py_(const char *type,
                   void *      libobs_in,
                   bool        ownership,
                   PyObject ** py_out,
                   const char *id,
                   const char *func,
                   int         line)
{
	swig_type_info *info = SWIG_TypeQuery(type);
	if (info == NULL) {
		warn("%s:%d: SWIG could not find type: %s%s%s",
		     func,
		     line,
		     id ? id : "",
		     id ? "::" : "",
		     type);
		return false;
	}

	*py_out = SWIG_NewPointerObj(libobs_in, info, (int)ownership);
	if (*py_out == Py_None) {
		warn("%s:%d: SWIG failed to convert obs object to python "
		     "object: %s%s%s",
		     func,
		     line,
		     id ? id : "",
		     id ? "::" : "",
		     type);
		return false;
	}

	return true;
}

#define libobs_to_py(type, obs_obj, ownership, py_obj) \
	libobs_to_py_(#type " *", obs_obj, ownership, py_obj, \
			py_src->py_source_info.id, __func__, __LINE__)

static inline bool py_call0(PyObject *call, PyObject **ret)
{
	PyObject *arg_list = Py_BuildValue("()");
	PyObject *ret_val  = PyObject_CallObject(call, arg_list);
	if (ret)
		*ret = ret_val;
	else
		Py_XDECREF(ret_val);
	Py_XDECREF(arg_list);
	return !py_error();
}

static inline bool py_callva(PyObject *call, PyObject **ret,
		const char *arg_def, va_list args)
{
	PyObject *arg_list = Py_VaBuildValue(arg_def, args);
	PyObject *ret_val  = PyObject_CallObject(call, arg_list);
	if (ret)
		*ret = ret_val;
	else
		Py_XDECREF(ret_val);
	Py_XDECREF(arg_list);
	return !py_error();
}

bool py_call(PyObject *call, PyObject **ret, const char *arg_def, ...)
{
	va_list args;
	bool    success;

	va_start(args, arg_def);
	success = py_callva(call, ret, arg_def, args);
	va_end(args);
	return success;
}

bool py_import_script(const char *name)
{
	PyObject *py_module    = NULL;
	PyObject *py_name      = NULL;
	PyObject *py_func_name = NULL;
	PyObject *py_func      = NULL;
	bool      success      = false;

	if (!name || !*name)
		return false;

	py_name   = PyUnicode_FromString(name);
	py_module = PyImport_Import(py_name);
	if (py_error() || !py_module) {
		warn("Failed to load module '%s'", name);
		goto fail;
	}

	PyModule_AddObject(py_module, "obspython", py_libobs);

	py_func_name = PyUnicode_FromString("obs_module_load");
	py_func      = PyObject_GetAttr(py_module, py_func_name);
	if (py_error() || !py_func) {
		warn("Failed to find obs_module_load in module '%s'", name);
		goto fail;
	}

	py_call0(py_func, NULL);
	if (py_error()) {
		warn("Failed to call obs_module_load from module '%s'", name);
		goto fail;
	}

	success = true;

fail:
	Py_XDECREF(py_func);
	Py_XDECREF(py_func_name);
	Py_XDECREF(py_name);
	Py_XDECREF(py_module);
	return success;
}

/* ========================================================================= */

static void *py_source_create(obs_data_t *settings, obs_source_t *source)
{
	struct python_data_pair *py_data       = NULL;
	PyObject *               data          = NULL;
	PyObject *               py_settings   = NULL;
	PyObject *               py_source_ctx = NULL;

	py_source_t *py_src = obs_source_get_type_data(source);

	lock_python();

	if (!func_callable(create))
		goto fail;
	if (!libobs_to_py(obs_data_t, settings, false, &py_settings))
		goto fail;
	if (!libobs_to_py(obs_source_t, settings, false, &py_source_ctx))
		goto fail;
	if (!py_call(py_src->create,
	             &data,
	             "(OO)",
	             py_settings,
	             py_source_ctx))
		goto fail;

	py_data         = bzalloc(sizeof(struct python_data_pair));
	py_data->data   = data;
	py_data->source = py_src;

	Py_INCREF(py_src);

fail:
	Py_XDECREF(py_settings);
	Py_XDECREF(py_source_ctx);
	unlock_python();
	return py_data;
}

static void py_source_destroy(void *data)
{
	struct python_data_pair *py_pair = data;
	py_source_t *            py_src  = py_pair->source;
	PyObject *               py_data = py_pair->data;

	lock_python();

	if (func_callable(destroy)) {
		if (!py_call(py_src->destroy, NULL, "(O)", py_data))
			warn("Error destroying python object '%s'",
					py_src->py_source_info.id);
	}

	Py_XDECREF(py_data);
	Py_XDECREF(py_src);

	unlock_python();

	bfree(data);
}

static uint32_t py_source_get_width(void *data)
{
	struct python_data_pair *py_pair = data;
	py_source_t *            py_src  = py_pair->source;
	PyObject *               py_data = py_pair->data;
	PyObject *               ret     = NULL;
	uint32_t                 width   = 0;

	lock_python();

	if (!func_callable(get_width))
		goto fail;
	if (!py_call(py_src->get_width, &ret, "(O)", py_data))
		goto fail;
	if (PyLong_Check(ret))
		width = (uint32_t)PyLong_AsLong(ret);

fail:
	Py_XDECREF(ret);
	unlock_python();

	return width;
}

static const char *py_source_get_name(void *type_data)
{
	py_source_t *py_src = type_data;
	PyObject    *ret    = NULL;

	lock_python();

	if (!func_callable(get_name))
		goto fail;
	if (!py_call0(py_src->get_name, &ret))
		goto fail;

	if (PyUnicode_Check(ret)) {
		char *utf8_name = PyUnicode_AsUTF8(ret);
		if (py_error() || utf8_name == NULL)
			goto fail;

		py_src->name = cstrcache_get(utf8_name);
	}

fail:
	Py_XDECREF(ret);
	unlock_python();
	return py_src->name;
}

static uint32_t py_source_get_height(void *data)
{
	struct python_data_pair *py_pair = data;
	py_source_t *            py_src  = py_pair->source;
	PyObject *               py_data = py_pair->data;
	PyObject *               ret     = NULL;
	long                     height  = 0;

	lock_python();

	if (!func_callable(get_height))
		goto fail;
	if (!py_call(py_src->get_height, &ret, "(O)", py_data))
		goto fail;
	if (PyLong_Check(ret))
		height = PyLong_AsLong(ret);

fail:
	Py_XDECREF(ret);
	unlock_python();

	return height;
}

static obs_properties_t *py_source_properties(void *data)
{
	struct python_data_pair *py_pair = data;
	py_source_t *            py_src  = py_pair->source;
	PyObject *               py_data = py_pair->data;
	obs_properties_t *       props   = NULL;
	PyObject *               ret     = NULL;

	lock_python();

	if (!func_callable(get_properties))
		goto fail;
	if (!py_call(py_src->get_properties, &ret, "(O)", py_data))
		goto fail;

	py_to_libobs(obs_properties_t, ret, &props);

fail:
	Py_XDECREF(ret);
	unlock_python();
	return props;
}

static void py_source_get_defaults(void *type_data, obs_data_t *settings)
{
	py_source_t *py_src      = type_data;
	PyObject *   py_settings = NULL;

	lock_python();

	if (!func_callable(get_defaults))
		goto fail;
	if (!libobs_to_py(obs_data_t, settings, false, &py_settings))
		goto fail;

	py_call(py_src->get_defaults, NULL, "(O)", py_settings);

fail:
	Py_XDECREF(py_settings);
	unlock_python();
}

static void py_source_update(void *data, obs_data_t *settings)
{
	struct python_data_pair *py_pair     = data;
	py_source_t *            py_src      = py_pair->source;
	PyObject *               py_data     = py_pair->data;
	PyObject *               py_settings = NULL;
	PyGILState_STATE         gstate      = PyGILState_Ensure();

	if (!func_callable(update))
		goto fail;
	if (!libobs_to_py(obs_data_t, settings, false, &py_settings))
		goto fail;

	py_call(py_src->update, NULL, "(OO)", py_data, py_settings);

fail:
	Py_XDECREF(py_settings);
	unlock_python();
}

static void py_source_activate(void *data)
{
	struct python_data_pair *py_pair = data;
	py_source_t *            py_src  = py_pair->source;
	PyObject *               py_data = py_pair->data;

	lock_python();

	if (func_callable(activate))
		py_call(py_src->activate, NULL, "(O)", py_data);

	unlock_python();
}

static void py_source_deactivate(void *data)
{
	struct python_data_pair *py_pair = data;
	py_source_t *            py_src  = py_pair->source;
	PyObject *               py_data = py_pair->data;

	lock_python();

	if (func_callable(deactivate))
		py_call(py_src->deactivate, NULL, "(O)", py_data);

	unlock_python();
}

static void py_source_show(void *data)
{
	struct python_data_pair *py_pair = data;
	py_source_t *            py_src  = py_pair->source;
	PyObject *               py_data = py_pair->data;

	lock_python();

	if (func_callable(show))
		py_call(py_src->show, NULL, "(O)", py_data);

	unlock_python();
}

static void py_source_hide(void *data)
{
	struct python_data_pair *py_pair = data;
	py_source_t *            py_src  = py_pair->source;
	PyObject *               py_data = py_pair->data;

	lock_python();

	if (func_callable(hide))
		py_call(py_src->hide, NULL, "(O)", py_data);

	unlock_python();
}

static void py_source_video_tick(void *data, float seconds)
{
	struct python_data_pair *py_pair = data;
	py_source_t *            py_src  = py_pair->source;
	PyObject *               py_data = py_pair->data;

	lock_python();

	if (func_callable(video_tick))
		py_call(py_src->video_tick,
		        NULL,
		        "(Of)",
		        py_data,
		        seconds);

	unlock_python();
}

static void py_source_video_render(void *data, gs_effect_t *effect)
{
	struct python_data_pair *py_pair   = data;
	py_source_t *            py_src    = py_pair->source;
	PyObject *               py_data   = py_pair->data;
	PyObject *               py_effect = NULL;

	lock_python();

	if (!func_callable(video_render))
		goto fail;
	if (!libobs_to_py(gs_effect_t, effect, false, &py_effect))
		goto fail;

	py_call(py_src->video_render, NULL, "(OO)", py_data, py_effect);

fail:
	Py_XDECREF(py_effect);
	unlock_python();
}

static struct obs_source_frame *
py_source_filter_video(void *data, struct obs_source_frame *frame)
{
	struct python_data_pair *py_pair   = data;
	py_source_t *            py_src    = py_pair->source;
	PyObject *               py_data   = py_pair->data;
	struct obs_source_frame *new_frame = NULL;
	PyObject *               py_frame  = NULL;
	PyObject *               py_ret    = NULL;

	lock_python();

	if (!func_callable(filter_video))
		goto fail;
	if (!libobs_to_py(obs_source_frame, frame, false, &py_frame))
		goto fail;
	if (!py_call(py_src->filter_video, &py_ret, "(OO)", py_data, py_frame))
		goto fail;

	py_to_libobs(obs_source_frame, py_ret, new_frame);

fail:
	if (!new_frame)
		new_frame = (void *)frame;

	Py_XDECREF(py_ret);
	Py_XDECREF(py_frame);
	unlock_python();
	return new_frame;
}

static struct obs_audio_data *
py_source_filter_audio(void *data, struct obs_audio_data *audio)
{
	struct python_data_pair *py_pair   = data;
	py_source_t *            py_src    = py_pair->source;
	PyObject *               py_data   = py_pair->data;
	struct obs_audio_data *  new_audio = NULL;
	PyObject *               py_ret    = NULL;
	PyObject *               py_audio  = NULL;

	lock_python();

	if (!func_callable(filter_audio))
		goto fail;
	if (!libobs_to_py(obs_audio_data, audio, false, &py_audio))
		goto fail;
	if (!py_call(py_src->filter_audio, &py_ret, "(O)", py_data))
		goto fail;

	py_to_libobs(obs_audio_data, py_ret, new_audio);

fail:
	if (!new_audio)
		new_audio = (void *)audio;

	Py_XDECREF(py_ret);
	Py_XDECREF(py_audio);
	unlock_python();
	return new_audio;
}

static void py_source_save(void *data, obs_data_t *settings)
{
	struct python_data_pair *py_pair     = data;
	py_source_t *            py_src      = py_pair->source;
	PyObject *               py_data     = py_pair->data;
	PyObject *               py_settings = NULL;

	lock_python();

	if (!func_callable(save))
		goto fail;
	if (!libobs_to_py(obs_data_t, settings, false, &py_settings))
		goto fail;

	py_call(py_src->save, NULL, "(OO)", py_data, py_settings);

fail:
	Py_XDECREF(py_settings);
	unlock_python();
}

static void py_source_load(void *data, obs_data_t *settings)
{
	struct python_data_pair *py_pair     = data;
	py_source_t *            py_src      = py_pair->source;
	PyObject *               py_data     = py_pair->data;
	PyObject *               py_settings = NULL;

	lock_python();

	if (!func_callable(load))
		goto fail;
	if (!libobs_to_py(obs_data_t, settings, false, &py_settings))
		goto fail;

	py_call(py_src->load, NULL, "(OO)", py_data, py_settings);

fail:
	Py_XDECREF(py_settings);
	unlock_python();
}

static void py_source_mouse_click(void *                        data,
                                  const struct obs_mouse_event *event,
                                  int32_t                       type,
                                  bool                          mouse_up,
                                  uint32_t                      click_count)
{
	struct python_data_pair *py_pair  = data;
	py_source_t *            py_src   = py_pair->source;
	PyObject *               py_data  = py_pair->data;
	PyObject *               py_event = NULL;

	lock_python();

	if (!func_callable(mouse_click))
		goto fail;
	if (!libobs_to_py(obs_mouse_event, (void *)event, false, &py_event))
		goto fail;

	py_call(py_src->mouse_click,
	        NULL,
	        "(OOipi)",
	        py_data,
	        py_event,
	        type,
	        mouse_up,
	        click_count);

fail:
	Py_XDECREF(py_event);
	unlock_python();
}

static void py_source_mouse_move(void *                        data,
                                 const struct obs_mouse_event *event,
                                 bool                          mouse_leave)
{
	struct python_data_pair *py_pair  = data;
	py_source_t *            py_src   = py_pair->source;
	PyObject *               py_data  = py_pair->data;
	PyObject *               py_event = NULL;

	lock_python();

	if (!func_callable(mouse_move))
		goto fail;
	if (!libobs_to_py(obs_mouse_event, (void *)event, false, &py_event))
		goto fail;

	py_call(py_src->create, NULL, "(OOp)", py_data, py_event, mouse_leave);

fail:
	Py_XDECREF(py_event);
	unlock_python();
}

static void py_source_mouse_wheel(void *                        data,
                                  const struct obs_mouse_event *event,
                                  int                           x_delta,
                                  int                           y_delta)
{
	struct python_data_pair *py_pair  = data;
	py_source_t *            py_src   = py_pair->source;
	PyObject *               py_data  = py_pair->data;
	PyObject *               py_event = NULL;

	lock_python();

	if (!func_callable(mouse_wheel))
		goto fail;
	if (!libobs_to_py(obs_mouse_event, (void *)event, false, &py_event))
		goto fail;

	py_call(py_src->mouse_wheel,
	        NULL,
	        "(OOii)",
	        py_data,
	        py_event,
	        x_delta,
	        y_delta);

fail:
	Py_XDECREF(py_event);
	unlock_python();
}

static void py_source_focus(void *data, bool focus)
{
	struct python_data_pair *py_pair = data;
	py_source_t *            py_src  = py_pair->source;
	PyObject *               py_data = py_pair->data;

	lock_python();

	if (func_callable(focus))
		py_call(py_src->focus, NULL, "(Op)", py_data, focus);

	unlock_python();
}

static void
py_source_key_click(void *data, const struct obs_key_event *event, bool key_up)
{
	struct python_data_pair *py_pair  = data;
	py_source_t *            py_src   = py_pair->source;
	PyObject *               py_data  = py_pair->data;
	PyObject *               py_event = NULL;

	lock_python();

	if (!func_callable(key_click))
		goto fail;
	if (!libobs_to_py(obs_key_event, (void *)event, false, &py_event))
		goto fail;

	py_call(py_src->key_click, NULL, "(OOp)", py_data, py_event, key_up);

fail:
	Py_XDECREF(py_event);
	unlock_python();
}

static void py_source_free_type_data(void *data)
{
	lock_python();
	Py_XDECREF(data);
	unlock_python();
}

/* ========================================================================= */

/* Only called in a python function and by us. */
void py_to_obs_source_info(py_source_t *py_info)
{
	/* Should probably check that no other source already has this id */
	struct obs_source_info *info = &py_info->py_source_info;
	info->id                     = PyUnicode_AsUTF8(py_info->id);
	info->type                   = PyLong_AsLong(py_info->type);
	info->output_flags           = PyLong_AsLong(py_info->flags);
	info->get_defaults2          = py_source_get_defaults;
	info->create                 = py_source_create;
	info->destroy                = py_source_destroy;
	info->get_name               = py_source_get_name;
	info->get_width              = py_source_get_width;
	info->get_height             = py_source_get_height;
	info->get_properties         = py_source_properties;
	info->update                 = py_source_update;
	info->activate               = py_source_activate;
	info->deactivate             = py_source_deactivate;
	info->show                   = py_source_show;
	info->hide                   = py_source_hide;
	info->video_tick             = py_source_video_tick;
	info->video_render           = py_source_video_render;
	info->filter_video           = py_source_filter_video;
	info->filter_audio           = py_source_filter_audio;
	info->save                   = py_source_save;
	info->load                   = py_source_load;
	info->mouse_click            = py_source_mouse_click;
	info->mouse_move             = py_source_mouse_move;
	info->mouse_wheel            = py_source_mouse_wheel;
	info->focus                  = py_source_focus;
	info->key_click              = py_source_key_click;
	info->type_data              = py_info;
	info->free_type_data         = py_source_free_type_data;
};

/* Python Type Object */
PyTypeObject py_source_type = {
        PyVarObject_HEAD_INIT(NULL, 0) "obspython.obs_source_info", /*tp_name*/
        sizeof(py_source_t),                      /*tp_basicsize*/
        0,                                        /*tp_itemsize*/
        (destructor)py_source_dealloc,            /*tp_dealloc*/
        0,                                        /*tp_print*/
        0,                                        /*tp_getattr*/
        0,                                        /*tp_setattr*/
        0,                                        /*tp_compare*/
        0,                                        /*tp_repr*/
        0,                                        /*tp_as_number*/
        0,                                        /*tp_as_sequence*/
        0,                                        /*tp_as_mapping*/
        0,                                        /*tp_hash */
        0,                                        /*tp_call*/
        0,                                        /*tp_str*/
        0,                                        /*tp_getattro*/
        0,                                        /*tp_setattro*/
        0,                                        /*tp_as_buffer*/
        Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
        "pylibobs Source",                        /*tp_doc */
        0,                                        /*tp_traverse */
        0,                                        /*tp_clear */
        0,                                        /*tp_richcompare */
        0,                                        /*tp_weaklistoffset */
        0,                                        /*tp_iter */
        0,                                        /*tp_iternext */
        py_source_methods,                        /*tp_methods */
        py_source_members,                        /*tp_members */
        0,                                        /*tp_getset */
        0,                                        /*tp_base */
        0,                                        /*tp_dict */
        0,                                        /*tp_descr_get */
        0,                                        /*tp_descr_set */
        0,                                        /*tp_dictoffset */
        (initproc)py_source_init,                 /*tp_init */
        0,                                        /*tp_alloc */
        py_source_new,                            /*tp_new */
};

PyObject *py_obs_get_script_config_path(PyObject *self, PyObject *args)
{
	Py_ssize_t argLength = PyTuple_Size(args);
	if (argLength != 0) {
		PyErr_SetString(PyExc_TypeError, "Wrong number of arguments");
		return NULL;
	}

	char *path = obs_module_config_path("");
	PyObject *py_path = PyUnicode_FromString(path);
	bfree(path);

	UNUSED_PARAMETER(self);
	return py_path;
}

PyObject *py_obs_register_source(PyObject *self, PyObject *args)
{
	UNUSED_PARAMETER(self);

	Py_ssize_t argLength = PyTuple_Size(args);
	if (argLength != 1) {
		PyErr_SetString(PyExc_TypeError, "Wrong number of arguments");
		return NULL;
	}

	PyObject *obj;

	if (!PyArg_ParseTuple(args, "O", &obj)) {
		return NULL;
	}

	if (!PyObject_TypeCheck(obj, &py_source_type)) {
		PyErr_SetString(PyExc_TypeError,
		                "Object is not "
		                "obspython.Source or subclass "
		                "of obspython.Source");
		return NULL;
	}

	py_source_t *py_src = (py_source_t *)obj;

	/* updates all the C calls to call the correct python wrapper funcs */
	py_to_obs_source_info(py_src);

	obs_register_source(&py_src->py_source_info);

	Py_INCREF(py_src);
	Py_RETURN_NONE;
}
