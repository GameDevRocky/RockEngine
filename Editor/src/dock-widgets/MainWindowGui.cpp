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
#include "dock-widgets/AnimatorGui.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "Editor.hpp"
#include <QOpenGLWidget>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent){}
MainWindow::~MainWindow(){SaveLayout();}

void MainWindow::Init()
{
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

    // Created and wired up now (so its selection subscription is set up in edit
    // mode, like the other panels), but not docked until ShowAnimator() is first
    // called from the Animator inspector.
    AnimatorGui::Get()->Init();

    setMenuBar(menu_bar);

    std::cout << "All Widgets Initialized" << std::endl;
    
    connect(file_explorer, &FileExplorerGui::RaiseFolderView, this, [this]() {
        folderViewDock->raise();
    });
    

    // The Scene/Game viewports live as dock widgets inside an inner QMainWindow
    // that is this window's central widget. Because that inner window has no
    // central widget, the two docks fill its entire area -- so they can be
    // tabbed together (the default, mimicking the old QTabWidget), dragged out
    // to float, split side-by-side, and re-docked in the center. Hierarchy /
    // Inspector / Console etc. remain docks of the OUTER window and surround it.
    viewportArea = new QMainWindow(this);
    viewportArea->setWindowFlags(Qt::Widget);          // embed, don't behave as a top-level window
    viewportArea->setDockNestingEnabled(true);          // allow splitting Scene | Game in the center
    viewportArea->setDockOptions(QMainWindow::AllowTabbedDocks | QMainWindow::AllowNestedDocks);

    sceneviewDock = new QDockWidget("Scene", viewportArea);
    sceneviewDock->setObjectName("SceneViewDock");
    sceneviewDock->setWidget(scene_view);
    // Movable + floatable, but not closable: closing would hide a viewport with
    // no menu path to bring it back. Undock via drag/double-click instead.
    sceneviewDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    viewportArea->addDockWidget(Qt::LeftDockWidgetArea, sceneviewDock);

    gameviewDock = new QDockWidget("Game", viewportArea);
    gameviewDock->setObjectName("GameViewDock");
    gameviewDock->setWidget(game_view);
    gameviewDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    viewportArea->addDockWidget(Qt::LeftDockWidgetArea, gameviewDock);

    // Start tabbed with Scene in front, matching the previous QTabWidget layout.
    viewportArea->tabifyDockWidget(sceneviewDock, gameviewDock);
    sceneviewDock->raise();

    setCentralWidget(viewportArea);

    // Keep the frame-loop driver in sync with whichever viewport is actually
    // visible (tab switch, float, or RuntimeBar raising the Game view on play)
    // -- see Editor::SetFrameDriver. QDockWidget::visibilityChanged fires when a
    // dock is raised to the front of a tab group, hidden behind one, floated, or
    // shown/hidden. Each handler falls back to the other viewport so we always
    // drive something that's on screen.
    connect(sceneviewDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (visible)                         Editor::Get()->SetFrameDriver(SceneViewGui::Get());
        else if (gameviewDock->isVisible())  Editor::Get()->SetFrameDriver(GameViewGui::Get());
    });
    connect(gameviewDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (visible)                          Editor::Get()->SetFrameDriver(GameViewGui::Get());
        else if (sceneviewDock->isVisible())  Editor::Get()->SetFrameDriver(SceneViewGui::Get());
    });

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
    //ClearLayout();
    std::cout << "Main Window Initialized " << std::endl;

}

void MainWindow::PostInit(){
    showMaximized();
    HierarchyGui::Get()->PostInit();
    MenuBar::Get()->PostInit();
    std::cout << "MainWindow Started" << std::endl;
}

void MainWindow::ShowAnimator(){
    // Lazily create the dock the first time it's opened, tabbed into the central
    // viewport area beside Scene/Game. Closable, since it's opened on demand.
    if (!animatorDock) {
        animatorDock = new QDockWidget("Animator", viewportArea);
        animatorDock->setObjectName("AnimatorDock");
        animatorDock->setWidget(AnimatorGui::Get());
        animatorDock->setFeatures(QDockWidget::DockWidgetMovable |
                                  QDockWidget::DockWidgetFloatable |
                                  QDockWidget::DockWidgetClosable);
        viewportArea->addDockWidget(Qt::LeftDockWidgetArea, animatorDock);
        viewportArea->tabifyDockWidget(sceneviewDock, animatorDock);
    }
    animatorDock->show();     // re-show if the user had closed it
    animatorDock->raise();    // bring the tab to the front (focus)
}

void MainWindow::SaveLayout()
{
    QSettings settings("Rocklyn", "RockEngineEditor");

    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
    // saveState() only serializes THIS window's docks; the Scene/Game docks
    // belong to the inner viewportArea, so persist its arrangement separately.
    if (viewportArea) settings.setValue("viewportState", viewportArea->saveState());
}

void MainWindow::LoadLayout()
{
    QSettings settings("Rocklyn", "RockEngineEditor");

    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
    if (viewportArea) viewportArea->restoreState(settings.value("viewportState").toByteArray());
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
    settings.remove("viewportState");
}
