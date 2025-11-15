#include "MainWindowGui.hpp"
#include <QDockWidget>
#include <QTabWidget>
#include <QWidget>

#include "ConsoleGui.hpp"
#include "GameViewGui.hpp"
#include "SceneViewGui.hpp"
#include "HierarchyGui.hpp"
#include "InspectorGui.hpp"
#include "FileExplorerGui.hpp"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
}

void MainWindow::Init()
{
    // --- Retrieve all widgets ---
    ConsoleGui& console_widget = *ConsoleGui::Get();
    GameViewGui& game_view = *GameViewGui::Get();
    SceneViewGui& scene_view = *SceneViewGui::Get();
    HierarchyGui& hierarchy = *HierarchyGui::Get();
    InspectorGui& inspector = *InspectorGui::Get();
    FileExplorerGui& file_explorer = *FileExplorerGui::Get();

    console_widget.Init();
    game_view.Init();
    scene_view.Init();
    hierarchy.Init();
    inspector.Init();
    file_explorer.Init();


    // --- Create central tabbed view for Scene and Game ---
    QTabWidget* central_tabs = new QTabWidget(this);
    central_tabs->addTab(&scene_view, "Scene");
    central_tabs->addTab(&game_view, "Game");
    setCentralWidget(central_tabs);

    QDockWidget* hierarchyDock = new QDockWidget("Hierarchy", this);
    hierarchyDock->setWidget(&hierarchy);
    addDockWidget(Qt::LeftDockWidgetArea, hierarchyDock);

    QDockWidget* inspectorDock = new QDockWidget("Inspector", this);
    inspectorDock->setWidget(&inspector);
    addDockWidget(Qt::RightDockWidgetArea, inspectorDock);

    QDockWidget* fileExplorerDock = new QDockWidget("File Explorer", this);
    fileExplorerDock->setWidget(&file_explorer);
    addDockWidget(Qt::BottomDockWidgetArea, fileExplorerDock);

    QDockWidget* consoleDock = new QDockWidget("Console", this);
    consoleDock->setWidget(&console_widget);
    addDockWidget(Qt::BottomDockWidgetArea, consoleDock);
    
    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);
    setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);


    setDockOptions(QMainWindow::AllowNestedDocks | QMainWindow::AllowTabbedDocks );

    // --- Window Setup ---
    setWindowTitle("My Game Engine");
    showMaximized();
}
