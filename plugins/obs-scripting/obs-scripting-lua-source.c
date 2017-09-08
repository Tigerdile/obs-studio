#include "obs-scripting-lua.h"
#include "cstrcache.h"

#include <obs-module.h>

/* ========================================================================= */

static int is_ptr(lua_State *script, int idx)
{
	return lua_isuserdata(script, idx) || lua_isnil(script, idx);
}

static int is_table(lua_State *script, int idx)
{
	return lua_istable(script, idx);
}

typedef int (*param_cb)(lua_State *script, int idx);

static inline bool verify_args1_(lua_State *script,
		param_cb param1_check,
		const char *func)
{
	if (lua_gettop(script) != 1) {
		warn("Wrong number of parameters for %s", func);
		return false;
	}
	if (!param1_check(script, 1)) {
		warn("Wrong parameter type for parameter %d of %s", 1, func);
		return false;
	}

	return true;
}

#define verify_args1(script, param1_check) \
	verify_args1_(script, param1_check, __FUNCTION__)

static inline const char *get_table_string_(lua_State *script, int idx,
		const char *name, const char *func)
{
	const char *str = "";

	lua_pushstring(script, name);
	lua_gettable(script, idx - 1);
	if (!lua_isstring(script, -1))
		warn("%s: no item '%s' of type %s", func, name, "string");
	else
		str = cstrcache_get(lua_tostring(script, -1));
	lua_pop(script, 1);

	return str;
}

static inline int get_table_int_(lua_State *script, int idx,
		const char *name, const char *func)
{
	int val = 0;

	lua_pushstring(script, name);
	lua_gettable(script, idx - 1);
	if (!lua_isinteger(script, -1))
		warn("%s: no item '%s' of type %s", func, name, "integer");
	else
		val = (int)lua_tointeger(script, -1);
	lua_pop(script, 1);

	return val;
}

static inline void get_callback_from_table_(lua_State *script, int idx,
		const char *name, int *p_reg_idx, const char *func)
{
	*p_reg_idx = LUA_REFNIL;

	lua_pushstring(script, name);
	lua_gettable(script, idx - 1);
	if (!lua_isfunction(script, -1)) {
		if (!lua_isnil(script, -1)) {
			warn("%s: item '%s' is not a function", func, name);
		}
		lua_pop(script, 1);
	} else {
		*p_reg_idx = luaL_ref(script, LUA_REGISTRYINDEX);
	}
}

#define get_table_string(script, idx, name) \
	get_table_string_(script, idx, name, __FUNCTION__)
#define get_table_int(script, idx, name) \
	get_table_int_(script, idx, name, __FUNCTION__)
#define get_callback_from_table(script, idx, name, p_reg_idx) \
	get_callback_from_table_(script, idx, name, p_reg_idx, __FUNCTION__)

static bool ls_get_libobs_obj_(lua_State * script,
                               const char *type,
                               int         lua_idx,
                               void *      libobs_out,
                               const char *id,
                               const char *func,
                               int         line)
{
	swig_type_info *info = SWIG_TypeQuery(script, type);
	if (info == NULL) {
		warn("%s:%d: SWIG could not find type: %s%s%s",
		     func,
		     line,
		     id ? id : "",
		     id ? "::" : "",
		     type);
		return false;
	}

	int ret = SWIG_ConvertPtr(script, lua_idx, libobs_out, info, 0);
	if (!SWIG_IsOK(ret)) {
		warn("%s:%d: SWIG failed to convert lua object to obs "
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

#define ls_get_libobs_obj(type, lua_index, obs_obj) \
	ls_get_libobs_obj_(ls->script, #type " *", lua_index, obs_obj, \
			ls->id, __FUNCTION__, __LINE__)

static bool ls_push_libobs_obj_(lua_State * script,
                                const char *type,
                                void *      libobs_in,
                                bool        ownership,
                                const char *id,
                                const char *func,
                                int         line)
{
	swig_type_info *info = SWIG_TypeQuery(script, type);
	if (info == NULL) {
		warn("%s:%d: SWIG could not find type: %s%s%s",
		     func,
		     line,
		     id ? id : "",
		     id ? "::" : "",
		     type);
		return false;
	}

	SWIG_NewPointerObj(script, libobs_in, info, (int)ownership);
	return true;
}

#define ls_push_libobs_obj(type, obs_obj, ownership) \
	ls_push_libobs_obj_(ls->script, #type " *", obs_obj, ownership, \
			ls->id, __FUNCTION__, __LINE__)

/* ========================================================================= */

struct obs_lua_source {
	lua_State * script;
	const char *id;
	const char *display_name;
	int         func_create;
	int         func_destroy;
	int         func_get_width;
	int         func_get_height;
	int         func_get_defaults;
	int         func_get_properties;
	int         func_update;
	int         func_activate;
	int         func_deactivate;
	int         func_show;
	int         func_hide;
	int         func_video_tick;
	int         func_video_render;
	int         func_save;
	int         func_load;
};

struct obs_lua_data {
	obs_source_t *         source;
	struct obs_lua_source *ls;
	int                    lua_data_ref;
};

static inline bool call_func_(lua_State *script,
		int reg_idx, int args, int rets,
		const char *func, const char *display_name)
{
	if (reg_idx == LUA_REFNIL)
		return false;

	lua_rawgeti(script, LUA_REGISTRYINDEX, reg_idx);
	lua_insert(script, -1 - args);

	if (lua_pcall(script, args, rets, 0) != 0) {
		warn("Failed to call %s for %s: %s", func, display_name,
				lua_tostring(script, -1));
		lua_pop(script, 1);
		return false;
	}

	return true;
}

#define have_func(name) \
	(ls->func_ ## name != LUA_REFNIL)
#define call_func(name, args, rets) \
	call_func_(ls->script, ls->func_ ## name, args, rets, #name, \
			ls->display_name)
#define ls_push_data() \
	lua_rawgeti(ls->script, LUA_REGISTRYINDEX, ld->lua_data_ref)
#define ls_pop(count) \
	lua_pop(ls->script, count)

static const char *obs_lua_source_get_name(void *type_data)
{
	struct obs_lua_source *ls = type_data;
	return ls->display_name;
}

static void *obs_lua_source_create(obs_data_t *settings, obs_source_t *source)
{
	struct obs_lua_source *ls = obs_source_get_type_data(source);

	if (!have_func(create))
		return NULL;

	if (!ls_push_libobs_obj(obs_data_t, settings, false))
		goto fail;
	if (!ls_push_libobs_obj(obs_source_t, source, false))
		goto fail;
	if (!call_func(create, 2, 1))
		goto fail;

	int lua_data_ref = luaL_ref(ls->script, LUA_REGISTRYINDEX);
	if (lua_data_ref == LUA_REFNIL)
		goto fail;

	struct obs_lua_data *data = bmalloc(sizeof(*data));
	data->source              = source;
	data->ls                  = ls;
	data->lua_data_ref        = lua_data_ref;
	return data;

fail:
	lua_settop(ls->script, 0);
	return NULL;
}

static void obs_lua_source_destroy(void *data)
{
	struct obs_lua_data *  ld = data;
	struct obs_lua_source *ls = ld->ls;

	if (!have_func(destroy))
		return;

	ls_push_data();
	call_func(destroy, 1, 0);
	luaL_unref(ls->script, LUA_REGISTRYINDEX, ld->lua_data_ref);

	bfree(data);
}

static uint32_t obs_lua_source_get_width(void *data)
{
	struct obs_lua_data *  ld    = data;
	struct obs_lua_source *ls    = ld->ls;
	uint32_t               width = 0;

	if (!have_func(get_width))
		return 0;

	ls_push_data();
	if (call_func(get_width, 1, 1)) {
		if (lua_isinteger(ls->script, -1))
			width = (uint32_t)lua_tointeger(ls->script, -1);
		ls_pop(1);
	}

	return width;
}

static uint32_t obs_lua_source_get_height(void *data)
{
	struct obs_lua_data *  ld     = data;
	struct obs_lua_source *ls     = ld->ls;
	uint32_t               height = 0;

	if (!have_func(get_height))
		return 0;

	ls_push_data();
	if (call_func(get_height, 1, 1)) {
		if (lua_isinteger(ls->script, -1))
			height = (uint32_t)lua_tointeger(ls->script, -1);
		ls_pop(1);
	}

	return height;
}

static void obs_lua_source_get_defaults(void *type_data, obs_data_t *settings)
{
	struct obs_lua_source *ls = type_data;

	if (!have_func(get_defaults))
		return;

	ls_push_libobs_obj(obs_data_t, settings, false);
	call_func(get_defaults, 1, 0);
}

static obs_properties_t *obs_lua_source_get_properties(void *data)
{
	struct obs_lua_data *  ld    = data;
	struct obs_lua_source *ls    = ld->ls;
	obs_properties_t *     props = NULL;

	if (!have_func(get_properties))
		return NULL;

	ls_push_data();
	if (call_func(get_properties, 1, 1)) {
		ls_get_libobs_obj(obs_properties_t, -1, &props);
		ls_pop(1);
	}

	return props;
}

static void obs_lua_source_update(void *data, obs_data_t *settings)
{
	struct obs_lua_data *  ld = data;
	struct obs_lua_source *ls = ld->ls;

	if (!have_func(update))
		return;

	ls_push_data();
	ls_push_libobs_obj(obs_data_t, settings, false);
	call_func(update, 2, 0);
}

#define DEFINE_VOID_DATA_CALLBACK(name) \
	static void obs_lua_source_ ## name(void *data) \
	{ \
		struct obs_lua_data *  ld = data; \
		struct obs_lua_source *ls = ld->ls; \
		if (!have_func(name)) \
			return; \
		ls_push_data(); \
		call_func(name, 1, 0); \
	}
DEFINE_VOID_DATA_CALLBACK(activate)
DEFINE_VOID_DATA_CALLBACK(deactivate)
DEFINE_VOID_DATA_CALLBACK(show)
DEFINE_VOID_DATA_CALLBACK(hide)
#undef DEFINE_VOID_DATA_CALLBACK

static void obs_lua_source_video_tick(void *data, float seconds)
{
	struct obs_lua_data *  ld = data;
	struct obs_lua_source *ls = ld->ls;

	if (!have_func(video_tick))
		return;

	ls_push_data();
	lua_pushnumber(ls->script, (double)seconds);
	call_func(video_tick, 2, 0);
}

static void obs_lua_source_video_render(void *data, gs_effect_t *effect)
{
	struct obs_lua_data *  ld = data;
	struct obs_lua_source *ls = ld->ls;

	if (!have_func(video_render))
		return;

	ls_push_data();
	ls_push_libobs_obj(gs_effect_t, effect, false);
	call_func(video_render, 2, 0);
}

static void obs_lua_source_save(void *data, obs_data_t *settings)
{
	struct obs_lua_data *  ld = data;
	struct obs_lua_source *ls = ld->ls;

	if (!have_func(save))
		return;

	ls_push_data();
	ls_push_libobs_obj(obs_data_t, settings, false);
	call_func(save, 2, 0);
}

static void obs_lua_source_load(void *data, obs_data_t *settings)
{
	struct obs_lua_data *  ld = data;
	struct obs_lua_source *ls = ld->ls;

	if (!have_func(load))
		return;

	ls_push_data();
	ls_push_libobs_obj(obs_data_t, settings, false);
	call_func(load, 2, 0);
}

static void obs_lua_source_free_type_data(void *type_data)
{
	struct obs_lua_source *ls = type_data;

#define unref(name) \
	luaL_unref(ls->script, LUA_REGISTRYINDEX, name)
	unref(ls->func_create);
	unref(ls->func_destroy);
	unref(ls->func_get_width);
	unref(ls->func_get_height);
	unref(ls->func_get_defaults);
	unref(ls->func_get_properties);
	unref(ls->func_update);
	unref(ls->func_activate);
	unref(ls->func_deactivate);
	unref(ls->func_show);
	unref(ls->func_hide);
	unref(ls->func_video_tick);
	unref(ls->func_video_render);
	unref(ls->func_save);
	unref(ls->func_load);
#undef unref

	bfree(ls);
}

static int obs_lua_register_source(lua_State *script)
{
	struct obs_lua_source ls = {0};
	struct obs_source_info info = {0};

	if (!verify_args1(script, is_table))
		goto fail;

	ls.script = script;
	ls.id     = get_table_string(script, -1, "id");

	info.id   = ls.id;
	info.type = (enum obs_source_type)get_table_int(script, -1, "type");

	info.output_flags = get_table_int(script, -1, "output_flags");

	lua_pushstring(script, "get_name");
	lua_gettable(script, -2);
	if (lua_pcall(script, 0, 1, 0) == 0) {
		ls.display_name = cstrcache_get(lua_tostring(script, -1));
		lua_pop(script, 1);
	}

	if (!ls.display_name ||
	    !*ls.display_name ||
	    !*info.id ||
	    !info.output_flags)
		goto fail;

#define get_callback(val) \
	do { \
		get_callback_from_table(script, -1, #val, &ls.func_ ## val); \
		info.val = obs_lua_source_ ## val; \
	} while (false)

	get_callback(create);
	get_callback(destroy);
	get_callback(get_width);
	get_callback(get_height);
	get_callback(get_properties);
	get_callback(update);
	get_callback(activate);
	get_callback(deactivate);
	get_callback(show);
	get_callback(hide);
	get_callback(video_tick);
	get_callback(video_render);
	get_callback(save);
	get_callback(load);
#undef get_callback

	get_callback_from_table(script, -1, "get_defaults",
			&ls.func_get_defaults);

	info.type_data      = bmemdup(&ls, sizeof(ls));
	info.free_type_data = obs_lua_source_free_type_data;
	info.get_name       = obs_lua_source_get_name;
	info.get_defaults2  = obs_lua_source_get_defaults;

	obs_register_source(&info);

fail:
	return 0;
}

/* ========================================================================= */

void add_lua_source_functions(lua_State *script)
{
	lua_getglobal(script, "obslua");
	if (!lua_istable(script, -1))
		return;

	lua_pushstring(script, "obs_register_source");
	lua_pushcfunction(script, obs_lua_register_source);
	lua_rawset(script, -3);

	lua_pop(script, 1);
}
