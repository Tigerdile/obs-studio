#include <util/base.h>
#include <util/platform.h>
#include <util/darray.h>
#include <util/dstr.h>

#include <obs-module.h>
#include "obs-scripting-python.h"

extern PyTypeObject py_source_type;

#define libobs_to_py(type, obs_obj, ownership, py_obj) \
	libobs_to_py_(#type " *", obs_obj, ownership, py_obj, \
			NULL, __func__, __LINE__)

/* ========================================================================= */

static PyMethodDef py_obs_methods[] = {
        {"obs_register_source",
         py_obs_register_source,
         METH_VARARGS,
         "Registers a new source"},
        {"obs_get_script_config_path",
         py_obs_get_script_config_path,
         METH_VARARGS,
         "Returns the path for script configuration files"},
	{0}
};

static void add_functions_to_py_module(PyObject *module,
		PyMethodDef *method_list)
{
	PyObject *dict = PyModule_GetDict(module);
	PyObject *name = PyModule_GetNameObject(module);
	if (!dict || !name) {
		return;
	}
	for (PyMethodDef *ml = method_list; ml->ml_name != NULL; ml++) {
		PyObject *func = PyCFunction_NewEx(ml, module, name);
		if (!func) {
			continue;
		}
		PyDict_SetItemString(dict, ml->ml_name, func);
		Py_DECREF(func);
	}
	Py_DECREF(name);
}

static void *extend_swig_libobs(PyObject *py_swig_libobs)
{
	if (PyType_Ready(&py_source_type) < 0) {
		return NULL;
	}

	Py_INCREF(&py_source_type);

	PyModule_AddObject(
	        py_swig_libobs, "obs_source_info", (PyObject*)&py_source_type);

	add_functions_to_py_module(py_swig_libobs, py_obs_methods);
	return NULL;
}

static void add_to_python_path(const char *path)
{
	PyObject *py_path_str = NULL;
	PyObject *py_path     = NULL;
	int       ret;

	ret = PyRun_SimpleString("import sys");
	if (py_error() || ret != 0)
		goto fail;

	/* borrowed reference here */
	py_path = PySys_GetObject("path");
	if (py_error() || !py_path)
		goto fail;

	py_path_str = PyUnicode_FromString(path);
	ret = PyList_Append(py_path, py_path_str);
	if (py_error() || ret != 0)
		goto fail;

fail:
	Py_XDECREF(py_path_str);
}

/* ========================================================================= */

PyObject *               py_libobs = NULL;
static DARRAY(PyObject*) plugin_scripts;

static PyObject *py_get_current_script_path(PyObject *self, PyObject *args)
{
	UNUSED_PARAMETER(args);
	return PyDict_GetItemString(PyModule_GetDict(self),
			"__script_dir__");
}

static void load_plugin_script(const char *file, const char *dir)
{
	PyObject *py_file     = NULL;
	PyObject *py_module   = NULL;
	PyObject *py_funcname = NULL;
	PyObject *py_func     = NULL;
	PyObject *py_success  = NULL;
	int       ret;

	py_file   = PyUnicode_FromString(file);
	py_module = PyImport_Import(py_file);
	if (py_error() || !py_module)
		goto fail;

	Py_XINCREF(py_libobs);
	ret = PyModule_AddObject(py_module, "obspython", py_libobs);
	if (py_error() || ret != 0)
		goto fail;

	ret = PyModule_AddStringConstant(py_module, "__script_dir__", dir);
	if (py_error() || ret != 0)
		goto fail;

	PyMethodDef global_funcs[] = {
		{"get_script_path",
		 py_get_current_script_path,
		 METH_NOARGS,
		 "Gets the script path"},
		{0}
	};

	add_functions_to_py_module(py_module, global_funcs);

	py_funcname = PyUnicode_FromString("obs_module_load");
	py_func     = PyObject_GetAttr(py_module, py_funcname);
	if (PyErr_Occurred() || !py_func)
		goto fail;

	py_success = PyObject_CallObject(py_func, NULL);
	if (py_error() || py_success == Py_False)
		goto fail;

	da_push_back(plugin_scripts, &py_module);

fail:
	Py_XDECREF(py_success);
	Py_XDECREF(py_func);
	Py_XDECREF(py_funcname);
	Py_XDECREF(py_module);
	Py_XDECREF(py_file);
}

static void unload_plugin_script(PyObject *py_module)
{
	PyObject *py_funcname = NULL;
	PyObject *py_func     = NULL;
	PyObject *py_ret      = NULL;

	py_funcname = PyUnicode_FromString("obs_module_unload");
	py_func     = PyObject_GetAttr(py_module, py_funcname);
	if (PyErr_Occurred() || !py_func)
		goto fail;

	py_ret = PyObject_CallObject(py_func, NULL);
	if (py_error())
		goto fail;

fail:
	Py_XDECREF(py_ret);
	Py_XDECREF(py_func);
	Py_XDECREF(py_funcname);
	Py_XDECREF(py_module);
}

static void load_plugin_scripts(const char *path)
{
	struct dstr script_search_path = {0};
	struct os_glob_info *glob;

	add_to_python_path(path);

	dstr_printf(&script_search_path, "%s/*.py", path);

	if (os_glob(script_search_path.array, 0, &glob) == 0) {
		for (size_t i = 0; i < glob->gl_pathc; i++) {
			const char *file       = glob->gl_pathv[i].path;
			const char *slash      = strrchr(file, '/');
			struct dstr short_file = {0};
			struct dstr dir        = {0};

			dstr_copy(&short_file, slash ? slash + 1 : file);
			dstr_resize(&short_file, short_file.len - 3);

			dstr_copy(&dir, file);
			dstr_resize(&dir, slash ? slash + 1 - file : 0);

			info("Loading Python plugin '%s'", file);
			load_plugin_script(short_file.array, dir.array);
			dstr_free(&short_file);
			dstr_free(&dir);
		}
		os_globfree(glob);
	}

	dstr_free(&script_search_path);
}

/* ========================================================================= */

// #define DEBUG_PYTHON_STARTUP

static const char *startup_script = "\n\
import sys\n\
import os\n\
import obspython\n\
class stdout_logger(object):\n\
	def write(self, message):\n\
		obspython.blog(obspython.LOG_INFO | obspython.LOG_TEXTBLOCK,\n\
				message)\n\
	def flush(self):\n\
		pass\n\
class stderr_logger(object):\n\
	def write(self, message):\n\
		obspython.blog(obspython.LOG_ERROR | obspython.LOG_TEXTBLOCK,\n\
				message)\n\
	def flush(self):\n\
		pass\n\
os.environ['PYTHONUNBUFFERED'] = '1'\n\
sys.stdout = stdout_logger()\n\
sys.stderr = stderr_logger()\n";

void python_unload(void);

void python_load(void)
{
	da_init(plugin_scripts);

	/* Use builtin python on windows */
#ifdef _WIN32
	struct dstr old_path  = {0};
	struct dstr new_path  = {0};
	char *      pythondir = obs_module_file("python");

	dstr_copy(&old_path, getenv("PATH"));

	if (pythondir && *pythondir) {
		dstr_printf(&new_path, "PYTHONHOME=%s", pythondir);
		_putenv(new_path.array);
		_putenv("PYTHONPATH=");
		_putenv("PATH=");
	}
#endif

	Py_Initialize();

#ifdef _WIN32
	if (pythondir && *pythondir) {
		dstr_printf(&new_path, "PATH=%s", old_path.array);
		_putenv(new_path.array);
	}

	bfree(pythondir);
	dstr_free(&new_path);
	dstr_free(&old_path);
#endif

	PyEval_InitThreads();

	/* ---------------------------------------------- */
	/* Must set arguments for guis to work            */

	wchar_t *argv[] = {L"", NULL};
	int      argc   = sizeof(argv) / sizeof(wchar_t*) - 1;

	PySys_SetArgv(argc, argv);

#ifdef DEBUG_PYTHON_STARTUP
	/* ---------------------------------------------- */
	/* Debug logging to file if startup is failing    */

	PyRun_SimpleString("import os");
	PyRun_SimpleString("import sys");
	PyRun_SimpleString("os.environ['PYTHONUNBUFFERED'] = '1'");
	PyRun_SimpleString("sys.stdout = open('./stdOut.txt','w',1)");
	PyRun_SimpleString("sys.stderr = open('./stdErr.txt','w',1)");
	PyRun_SimpleString("print(sys.version)");
#endif

	/* ---------------------------------------------- */
	/* Initialize Python dep script paths             */

	const char *data_path = obs_get_module_data_path(obs_current_module());

	struct dstr scripts_path = {0};
	dstr_copy(&scripts_path, data_path);
#if ARCH_BITS == 64
	dstr_cat(&scripts_path, "/64bit");
#else
	dstr_cat(&scripts_path, "/32bit");
#endif

	add_to_python_path(scripts_path.array);

	/* --------------- */

	const char **path = obs_get_parsed_search_paths("dep_scripts");
	while (*path) {
		add_to_python_path(*path);
		++path;
	}

	/* ---------------------------------------------- */
	/* Load main interface module                     */

	py_libobs = PyImport_ImportModule("obspython");
	bool success        = !py_error();
	if (!success) {
		warn("Error importing '%s/obspython.py', unloading obs-python",
		     scripts_path.array);
		goto out;
	}

	extend_swig_libobs(py_libobs);
	success = PyRun_SimpleString(startup_script) == 0;
	py_error();

out:
	/* ---------------------------------------------- */
	/* Free data                                      */

	dstr_free(&scripts_path);
	PyEval_ReleaseThread(PyGILState_GetThisThreadState());

	if (!success) {
		warn("Failed to load python plugin");
		python_unload();
	}
}

void python_post_maload(void)
{
	if (!Py_IsInitialized())
		return;

	/* ---------------------------------------------- */
	/* Load Python plugins                            */

	lock_python();

	char *builtin_script_path = obs_module_file("scripts");
	if (builtin_script_path) {
		load_plugin_scripts(builtin_script_path);
		bfree(builtin_script_path);
	}

	const char **paths = obs_get_parsed_search_paths("plugin_scripts");
	while (*paths) {
		load_plugin_scripts(*paths);
		paths++;
	}

	unlock_python();
}

void python_unload(void)
{
	if (Py_IsInitialized()) {
		PyGILState_Ensure();

		for (size_t i = 0; i < plugin_scripts.num; i++) {
			PyObject *script = plugin_scripts.array[i];
			unload_plugin_script(script);
		}

		Py_XDECREF(py_libobs);
		Py_Finalize();
	}

	da_free(plugin_scripts);
}
