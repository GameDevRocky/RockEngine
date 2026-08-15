#pragma once
#include <QMainWindow>
#include <QSettings>
#include <QDockWidget>
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
    void PostInit();
    void Shutdown();
    void ClearLayout();

    // Show the Animator editor panel, creating its dock the first time (tabbed
    // into the central viewport area next to Scene/Game) and raising it to focus.
    // Called from the Animator component's inspector "Open Animator" button.
    void ShowAnimator();

    // Inner QMainWindow used as this window's central widget. It has no central
    // widget of its own, so the Scene/Game dock widgets fill its whole area and
    // can be tabbed, split, floated, and re-docked "in the center" -- something
    // a top-level QMainWindow can't do (its center is reserved for the central
    // widget, docks only surround it).
    QMainWindow* viewportArea;
    QDockWidget* sceneviewDock;
    QDockWidget* gameviewDock;
    QDockWidget* hierarchyDock;
    QDockWidget* inspectorDock;
    QDockWidget* fileExplorerDock;
    QDockWidget* folderViewDock;
    QDockWidget* consoleDock;
    QDockWidget* aiAssistantDock;
    QDockWidget* runtimeBarDock;
    QDockWidget* animatorDock = nullptr;   // created lazily by ShowAnimator()

    class SceneViewGui* scene_view;
    class GameViewGui* game_view;
    
private:
    explicit MainWindow(QWidget* parent = nullptr);
    void LoadLayout();
    void SaveLayout();
    ~MainWindow();



};
