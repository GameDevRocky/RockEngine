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

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent){}
MainWindow::~MainWindow(){SaveLayout();}

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
    inspectorDock->setObjectName("InspectorDock");


    QDockWidget* fileExplorerDock = new QDockWidget("File Explorer", this);
    fileExplorerDock->setWidget(&file_explorer);
    addDockWidget(Qt::BottomDockWidgetArea, fileExplorerDock);
    fileExplorerDock->setObjectName("FileExplorerDock");


    QDockWidget* consoleDock = new QDockWidget("Console", this);
    consoleDock->setWidget(&console_widget);
    addDockWidget(Qt::BottomDockWidgetArea, consoleDock);
    consoleDock->setObjectName("ConsoleDock");

    
    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);
    setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);


    setDockOptions(QMainWindow::AllowNestedDocks | QMainWindow::AllowTabbedDocks );
    
    LoadLayout();

    setWindowTitle("My Game Engine");
    showMaximized();
}

void MainWindow::SaveLayout()
{
    QSettings settings("Rocklyn", "RockEngineEditor");

    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
}

void MainWindow::LoadLayout()
{
    QSettings settings("Rocklyn", "RockEngineEditor");

    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
}

void MainWindow::Shutdown(){
    SaveLayout();
    ClearLayout();
}


void MainWindow::ClearLayout()
{
    QSettings settings("Rocklyn", "RockEngineEditor");
    settings.remove("geometry");
    settings.remove("windowState");
}
