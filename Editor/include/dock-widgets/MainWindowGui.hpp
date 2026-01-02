#pragma once
#include <QMainWindow>
#include <QSettings>
#include <QDockWidget>
#include <QTabWidget>
#include <QWidget>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    static MainWindow* Get() {
        static MainWindow* instance = nullptr;
        if (!instance) {
            instance = new MainWindow(nullptr);
        }
        return instance;
    }
    void Init();
    void Start();
    void Shutdown();
    void ClearLayout();
    QTabWidget* central_tabs;
    QDockWidget* hierarchyDock;
    QDockWidget* inspectorDock;
    QDockWidget* fileExplorerDock;
    QDockWidget* folderViewDock;
    QDockWidget* consoleDock;
    class SceneViewGui* scene_view;
    class GameViewGui* game_view;
    
private:
    explicit MainWindow(QWidget* parent = nullptr);
    void LoadLayout();
    void SaveLayout();
    ~MainWindow();



};
