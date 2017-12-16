#include <QWidget>

class Ui_ScriptsTool;

class ScriptsTool : public QWidget {
	Q_OBJECT

	Ui_ScriptsTool *ui;

public:
	ScriptsTool();
	~ScriptsTool();

	void RemoveScript(const char *path);
	void ReloadScript(const char *path);
	void RefreshLists();

public slots:
	void on_close_clicked();

	void on_addScripts_clicked();
	void on_removeScripts_clicked();
	void on_reloadScripts_clicked();

	void on_addLuaDepPath_clicked();
	void on_removeLuaDepPath_clicked();

	void on_pythonPathBrowse_clicked();
};
