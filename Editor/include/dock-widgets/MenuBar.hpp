#pragma once

#include <QAction>
#include <QMenu>
#include <QMenuBar>

class MenuBar : public QMenuBar {
    Q_OBJECT

public:
    static MenuBar* Get() {
        static MenuBar* instance = nullptr;
        if (!instance) {
            instance = new MenuBar(nullptr);
        }
        return instance;
    }
    void Init();
    // Wires the Undo/Redo actions to the engine's UndoSystem. Separate from Init
    // because it needs the engine container, which only exists after Engine::Init.
    void PostInit();
    // Registers a dock's visibility toggle in Window after MainWindow creates
    // the dock. Ownership remains with the dock/QMainWindow.
    void AddWindowAction(QAction* action);

signals:
    void NewSceneRequested();
    void OpenSceneRequested();
    void SaveSceneRequested();
    void SaveSceneAsRequested();
    void ExitRequested();
    // No UndoRequested/RedoRequested: undoAction/redoAction come from
    // QUndoStack::createUndoAction and are already connected to the stack.
    // Routing them through a signal would invite a second, competing path.
    void ResetLayoutRequested();
    void AboutRequested();

private:
    explicit MenuBar(QWidget* parent = nullptr);
    ~MenuBar() override = default;

    // (Re)subscribes to the active container's UndoSystem. Must run again after
    // each play-mode swap: the containers own separate UndoSystem instances.
    void SubscribeToUndoSystem();
    // Pushes the current history state onto the two QActions: enabled/disabled
    // plus the "Undo Change Rotation" style text.
    void RefreshUndoActions();

    bool initialized_ = false;
    int m_undoSubId = -1;

    QMenu* fileMenu = nullptr;
    QMenu* editMenu = nullptr;
    QMenu* windowMenu = nullptr;
    QMenu* helpMenu = nullptr;

    QAction* newSceneAction = nullptr;
    QAction* openSceneAction = nullptr;
    QAction* saveSceneAction = nullptr;
    QAction* saveSceneAsAction = nullptr;
    QAction* buildGameAction = nullptr;
    QAction* exitAction = nullptr;

    QAction* undoAction = nullptr;
    QAction* redoAction = nullptr;

    QAction* resetLayoutAction = nullptr;

    QAction* aboutAction = nullptr;
};
