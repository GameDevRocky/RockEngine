#include "dock-widgets/MenuBar.hpp"

MenuBar::MenuBar(QWidget* parent) : QMenuBar(parent) {}

void MenuBar::Init() {
    if (initialized_) {
        return;
    }

    fileMenu = addMenu("File");
    editMenu = addMenu("Edit");
    windowMenu = addMenu("Window");
    helpMenu = addMenu("Help");

    newSceneAction = fileMenu->addAction("New Scene");
    openSceneAction = fileMenu->addAction("Open Scene");
    saveSceneAction = fileMenu->addAction("Save Scene");
    saveSceneAsAction = fileMenu->addAction("Save Scene As...");

    fileMenu->addSeparator();
    exitAction = fileMenu->addAction("Exit");

    undoAction = editMenu->addAction("Undo");
    redoAction = editMenu->addAction("Redo");

    resetLayoutAction = windowMenu->addAction("Reset Layout");

    aboutAction = helpMenu->addAction("About RockEngine");

    connect(newSceneAction, &QAction::triggered, this, &MenuBar::NewSceneRequested);
    connect(openSceneAction, &QAction::triggered, this, &MenuBar::OpenSceneRequested);
    connect(saveSceneAction, &QAction::triggered, this, &MenuBar::SaveSceneRequested);
    connect(saveSceneAsAction, &QAction::triggered, this, &MenuBar::SaveSceneAsRequested);
    connect(exitAction, &QAction::triggered, this, &MenuBar::ExitRequested);
    connect(undoAction, &QAction::triggered, this, &MenuBar::UndoRequested);
    connect(redoAction, &QAction::triggered, this, &MenuBar::RedoRequested);
    connect(resetLayoutAction, &QAction::triggered, this, &MenuBar::ResetLayoutRequested);
    connect(aboutAction, &QAction::triggered, this, &MenuBar::AboutRequested);

    initialized_ = true;
}
