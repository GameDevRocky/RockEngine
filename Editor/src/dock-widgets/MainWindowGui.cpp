#include "dock-widgets/MainWindowGui.hpp"
#include "dock-widgets/ConsoleGui.hpp"
#include "dock-widgets/GameViewGui.hpp"
#include "dock-widgets/SceneViewGui.hpp"
#include "dock-widgets/HierarchyGui.hpp"
#include "dock-widgets/InspectorGui.hpp"
#include "dock-widgets/FileExplorerGui.hpp"
#include "dock-widgets/FolderViewGui.hpp"

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent){}
MainWindow::~MainWindow(){SaveLayout();}

void MainWindow::Init()
{
    // --- Retrieve all widgets ---
    ConsoleGui* console_widget = ConsoleGui::Get();
    GameViewGui* game_view = GameViewGui::Get();
    SceneViewGui* scene_view = SceneViewGui::Get();
    HierarchyGui* hierarchy = HierarchyGui::Get();
    InspectorGui* inspector = InspectorGui::Get();
    FileExplorerGui* file_explorer = FileExplorerGui::Get();
    FolderViewGui* folder_view = FolderViewGui::Get();
    

    console_widget->Init();
    game_view->Init();
    scene_view->Init();
    hierarchy->Init();
    inspector->Init();
    file_explorer->Init();
    folder_view->Init();
    

    // Connect FileExplorer's RaiseFolderView signal to raise the dock
    connect(file_explorer, &FileExplorerGui::RaiseFolderView, this, [this]() {
        folderViewDock->raise();
    });
    

    central_tabs = new QTabWidget(this);
    
    central_tabs->addTab(scene_view, "Scene");
    central_tabs->addTab(game_view, "Game");
    setCentralWidget(central_tabs);

    hierarchyDock = new QDockWidget("Hierarchy", this);
    hierarchyDock->setWidget(hierarchy);
    addDockWidget(Qt::LeftDockWidgetArea, hierarchyDock);
  

    inspectorDock = new QDockWidget("Inspector", this);
    inspectorDock->setWidget(inspector);
    addDockWidget(Qt::RightDockWidgetArea, inspectorDock);
    inspectorDock->setObjectName("InspectorDock");


    fileExplorerDock = new QDockWidget("File Explorer", this);
    fileExplorerDock->setWidget(file_explorer);
    addDockWidget(Qt::BottomDockWidgetArea, fileExplorerDock);
    fileExplorerDock->setObjectName("FileExplorerDock");

    folderViewDock = new QDockWidget("Folder View", this);
    folderViewDock->setWidget(folder_view);
    addDockWidget(Qt::BottomDockWidgetArea, folderViewDock);
    folderViewDock->setObjectName("FolderViewDock");


    consoleDock = new QDockWidget("Console", this);
    consoleDock->setWidget(console_widget);
    addDockWidget(Qt::BottomDockWidgetArea, consoleDock);
    consoleDock->setObjectName("ConsoleDock");

    tabifyDockWidget(consoleDock, folderViewDock);
    

    
    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);
    setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);


    setDockOptions(QMainWindow::AllowNestedDocks | QMainWindow::AllowTabbedDocks );
    
    LoadLayout();
    
    setWindowTitle("My Game Engine");
}

void MainWindow::Start(){
    HierarchyGui::Get()->Start();
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
