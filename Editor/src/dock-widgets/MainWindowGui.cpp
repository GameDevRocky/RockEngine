#include "dock-widgets/MainWindowGui.hpp"
#include "dock-widgets/ConsoleGui.hpp"
#include "dock-widgets/GameViewGui.hpp"
#include "dock-widgets/SceneViewGui.hpp"
#include "dock-widgets/HierarchyGui.hpp"
#include "dock-widgets/InspectorGui.hpp"
#include "dock-widgets/FileExplorerGui.hpp"
#include "dock-widgets/FolderViewGui.hpp"
#include "dock-widgets/RuntimeBar.hpp"
#include "dock-widgets/MenuBar.hpp"
#include "engine/rendering/core/SharedResources.hpp"

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
    RuntimeBar* runtime_bar = RuntimeBar::Get();
    MenuBar* menu_bar = MenuBar::Get();

    console_widget->Init();
    game_view->Init();
    scene_view->Init();
    hierarchy->Init();
    inspector->Init();
    file_explorer->Init();
    folder_view->Init();
    runtime_bar->Init();
    menu_bar->Init();

    setMenuBar(menu_bar);

    std::cout << "All Widgets Initialized" << std::endl;
    
    connect(file_explorer, &FileExplorerGui::RaiseFolderView, this, [this]() {
        folderViewDock->raise();
    });
    

    central_tabs = new QTabWidget(this);
    
    central_tabs->addTab(scene_view, "Scene");
    central_tabs->addTab(game_view, "Game");
    setCentralWidget(central_tabs);

    hierarchyDock = new QDockWidget("Hierarchy", this);
    hierarchyDock->setWidget(hierarchy);
    hierarchyDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::BottomDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, hierarchyDock);
  

    inspectorDock = new QDockWidget("Inspector", this);
    inspectorDock->setWidget(inspector);
    inspectorDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, inspectorDock);
    inspectorDock->setObjectName("InspectorDock");


    fileExplorerDock = new QDockWidget("File Explorer", this);
    fileExplorerDock->setWidget(file_explorer);
    fileExplorerDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, fileExplorerDock);
    fileExplorerDock->setObjectName("FileExplorerDock");

    folderViewDock = new QDockWidget("Folder View", this);
    folderViewDock->setWidget(folder_view);
    folderViewDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, folderViewDock);
    folderViewDock->setObjectName("FolderViewDock");

    runtimeBarDock = new QDockWidget("", this);
    runtimeBarDock->setWidget(runtime_bar);
    runtimeBarDock->setAllowedAreas(Qt::TopDockWidgetArea);
    runtimeBarDock->setContentsMargins(0, 0, 0, 0);
    runtimeBarDock->setTitleBarWidget(new QWidget());
    addDockWidget(Qt::TopDockWidgetArea, runtimeBarDock);
    runtimeBarDock->setFeatures(QDockWidget::NoDockWidgetFeatures);

    consoleDock = new QDockWidget("Console", this);
    consoleDock->setWidget(console_widget);
    consoleDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, consoleDock);
    consoleDock->setObjectName("Console");

    tabifyDockWidget(consoleDock, folderViewDock);

    resizeDocks({fileExplorerDock}, {200}, Qt::Vertical);

    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);
    setCorner(Qt::TopRightCorner, Qt::TopDockWidgetArea);
    setDockNestingEnabled(false);
    setDockOptions(QMainWindow::AllowTabbedDocks | QMainWindow::ForceTabbedDocks);
    LoadLayout();

    std::cout << "Main Window Initialized " << std::endl;

}

void MainWindow::PostInit(){
    showMaximized();
    HierarchyGui::Get()->PostInit();
    std::cout << "MainWindow Started" << std::endl;
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
    //ClearLayout();
}


void MainWindow::ClearLayout()
{
    QSettings settings("Rocklyn", "RockEngineEditor");
    settings.remove("geometry");
    settings.remove("windowState");
}
