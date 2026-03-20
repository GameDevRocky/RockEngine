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

signals:
    void NewSceneRequested();
    void OpenSceneRequested();
    void SaveSceneRequested();
    void SaveSceneAsRequested();
    void ExitRequested();
    void UndoRequested();
    void RedoRequested();
    void ResetLayoutRequested();
    void AboutRequested();

private:
    explicit MenuBar(QWidget* parent = nullptr);
    ~MenuBar() override = default;

    bool initialized_ = false;

    QMenu* fileMenu = nullptr;
    QMenu* editMenu = nullptr;
    QMenu* windowMenu = nullptr;
    QMenu* helpMenu = nullptr;

    QAction* newSceneAction = nullptr;
    QAction* openSceneAction = nullptr;
    QAction* saveSceneAction = nullptr;
    QAction* saveSceneAsAction = nullptr;
    QAction* exitAction = nullptr;

    QAction* undoAction = nullptr;
    QAction* redoAction = nullptr;

    QAction* resetLayoutAction = nullptr;

    QAction* aboutAction = nullptr;
};
