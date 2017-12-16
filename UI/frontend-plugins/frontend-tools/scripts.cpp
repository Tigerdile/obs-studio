#include "scripts.hpp"

#include <QFileDialog>

#include <obs.hpp>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <obs-scripting.h>

#include <string>

#include "ui_scripts.h"

/* ----------------------------------------------------------------- */

using OBSScript = OBSObj<obs_script_t*, obs_script_destroy>;

struct ScriptData {
	std::vector<OBSScript> scripts;

	bool ScriptOpened(const char *path)
	{
		for (OBSScript &script : scripts) {
			const char *script_path = obs_script_get_path(script);
			if (strcmp(script_path, path) == 0) {
				return true;
			}
		}

		return false;
	}
};

static ScriptData *scriptData = nullptr;
static ScriptsTool *scriptsWindow = nullptr;

/* ----------------------------------------------------------------- */

ScriptsTool::ScriptsTool()
	: QWidget (nullptr),
	  ui      (new Ui_ScriptsTool)
{
	ui->setupUi(this);
	RefreshLists();
}

ScriptsTool::~ScriptsTool()
{
	delete ui;
	scriptsWindow = false;
}

void ScriptsTool::RemoveScript(const char *path)
{
	for (size_t i = 0; i < scriptData->scripts.size(); i++) {
		OBSScript &script = scriptData->scripts[i];

		const char *script_path = obs_script_get_path(script);
		if (strcmp(script_path, path) == 0) {
			scriptData->scripts.erase(
					scriptData->scripts.begin() + i);
			break;
		}
	}
}

void ScriptsTool::ReloadScript(const char *path)
{
	for (OBSScript &script : scriptData->scripts) {
		const char *script_path = obs_script_get_path(script);
		if (strcmp(script_path, path) == 0) {
			obs_script_reload(script);
			break;
		}
	}
}

void ScriptsTool::RefreshLists()
{
	ui->scripts->clear();

	for (OBSScript &script : scriptData->scripts) {
		const char *script_path = obs_script_get_path(script);
		ui->scripts->addItem(script_path);
	}
}

void ScriptsTool::on_close_clicked()
{
	close();
}

void ScriptsTool::on_addScripts_clicked()
{
	const char **formats = obs_scripting_supported_formats();
	const char **cur_format = formats;
	QString extensions;
	QString filter;

	while (*cur_format) {
		if (!extensions.isEmpty())
			extensions += QStringLiteral(" ");

		extensions += QStringLiteral("*.");
		extensions += *cur_format;

		cur_format++;
	}

	if (!extensions.isEmpty()) {
		filter += obs_module_text("FileFilter.ScriptFiles");
		filter += QStringLiteral(" (");
		filter += extensions;
		filter += QStringLiteral(")");
	}

	if (filter.isEmpty())
		return;

	QFileDialog dlg(this, obs_module_text("AddScripts"));
	dlg.setFileMode(QFileDialog::ExistingFiles);
	dlg.setNameFilter(filter);
	dlg.exec();

	QStringList files = dlg.selectedFiles();

	for (const QString &file : files) {
		QByteArray pathBytes = file.toUtf8();
		const char *path = pathBytes.constData();

		if (scriptData->ScriptOpened(path)) {
			continue;
		}

		obs_script_t *script = obs_script_create(path);
		if (script) {
			scriptData->scripts.emplace_back(script);
			ui->scripts->addItem(file);
		}
	}
}

void ScriptsTool::on_removeScripts_clicked()
{
	QList<QListWidgetItem *> items = ui->scripts->selectedItems();

	for (QListWidgetItem *item : items)
		RemoveScript(item->text().toUtf8().constData());
	RefreshLists();
}

void ScriptsTool::on_reloadScripts_clicked()
{
	QList<QListWidgetItem *> items = ui->scripts->selectedItems();
	for (QListWidgetItem *item : items)
		ReloadScript(item->text().toUtf8().constData());
}

void ScriptsTool::on_addLuaDepPath_clicked()
{
}

void ScriptsTool::on_removeLuaDepPath_clicked()
{
}

void ScriptsTool::on_pythonPathBrowse_clicked()
{
}

/* ----------------------------------------------------------------- */

extern "C" void FreeScripts()
{
	obs_scripting_unload();
}

static void obs_event(enum obs_frontend_event event, void *)
{
	if (event == OBS_FRONTEND_EVENT_EXIT) {
		delete scriptsWindow;
		delete scriptData;

	} else if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP) {
		delete scriptData;
		scriptData = new ScriptData;
	}
}

static void save_script_data(obs_data_t *save_data, bool saving, void *)
{
	if (saving) {
		obs_data_array_t *array = obs_data_array_create();

		for (OBSScript &script : scriptData->scripts) {
			const char *script_path = obs_script_get_path(script);

			obs_data_t *obj = obs_data_create();
			obs_data_set_string(obj, "path", script_path);
			obs_data_array_push_back(array, obj);
			obs_data_release(obj);
		}

		obs_data_set_array(save_data, "scripts-tool", array);
		obs_data_array_release(array);
	} else {
		obs_data_array_t *array = obs_data_get_array(save_data,
				"scripts-tool");

		delete scriptData;
		scriptData = new ScriptData;

		size_t size = obs_data_array_count(array);
		for (size_t i = 0; i < size; i++) {
			obs_data_t *obj = obs_data_array_item(array, i);
			const char *path = obs_data_get_string(obj, "path");

			obs_script_t *script = obs_script_create(path);
			if (script) {
				scriptData->scripts.emplace_back(script);
			}

			obs_data_release(obj);
		}

		if (scriptsWindow)
			scriptsWindow->RefreshLists();

		obs_data_array_release(array);
	}
}

extern "C" void InitScripts()
{
	obs_scripting_load();

	QAction *action = (QAction*)obs_frontend_add_tools_menu_qaction(
			obs_module_text("Scripts"));

	scriptData = new ScriptData;

	auto cb = [] ()
	{
		obs_frontend_push_ui_translation(obs_module_get_string);

		if (!scriptsWindow) {
			scriptsWindow = new ScriptsTool();
			scriptsWindow->show();
		} else {
			scriptsWindow->show();
			scriptsWindow->raise();
		}

		obs_frontend_pop_ui_translation();
	};

	obs_frontend_add_save_callback(save_script_data, nullptr);
	obs_frontend_add_event_callback(obs_event, nullptr);

	action->connect(action, &QAction::triggered, cb);
}
