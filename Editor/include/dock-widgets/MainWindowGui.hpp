#pragma once
#include <QMainWindow>
#include <QSettings>
#include <QDockWidget>
#include <QWidget>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    static MainWindow* Get() {
        MainWindow*& instance = Instance();
        if (!instance) {
            instance = new MainWindow(nullptr);
        }
        return instance;
    }
    static void Destroy() {
        MainWindow*& instance = Instance();
        delete instance;
        instance = nullptr;
    }
    void Init();
    void PostInit();
    void Shutdown();
    void ClearLayout();

    // Show the Animator editor panel, creating its dock the first time (tabbed
    // into the central viewport area next to Scene/Game) and raising it to focus.
    // Called from the Animator component's inspector "Open Animator" button.
    void ShowAnimator();

    // Retarget and raise the reusable Sprite Editor dock in the central viewport area.
    void ShowSpriteEditor(class Texture2D* texture);
    // Keep an existing Sprite Editor synchronized with asset selection without
    // creating or raising the dock solely because a texture was selected.
    void UpdateSpriteEditorTexture(class Texture2D* texture);

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
    QDockWidget* settingsDock;
    QDockWidget* animatorDock = nullptr;   // created lazily by ShowAnimator()
    QDockWidget* spriteEditorDock = nullptr; // created lazily by ShowSpriteEditor()

    class SceneViewGui* scene_view;
    class GameViewGui* game_view;
    
private:
    static MainWindow*& Instance() {
        static MainWindow* instance = nullptr;
        return instance;
    }
    explicit MainWindow(QWidget* parent = nullptr);
    void LoadLayout();
    void SaveLayout();
    ~MainWindow();

    class SpriteEditor* spriteEditor = nullptr;

};
