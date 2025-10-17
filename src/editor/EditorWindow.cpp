#include "EditorWindow.hpp"
#include <QMenuBar>
#include <QDockWidget>
#include <QLabel>
#include <QStatusBar>
#include <QToolBar>
#include <QTextEdit>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <QSettings>
#include <QCloseEvent>

EditorWindow::EditorWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("RockEngine Editor");
    resize(1280, 720);
}

void EditorWindow::Init() {
    SetupMenuBar();
    SetupToolBar();
    SetupStatusBar();
    SetupDocks();
    LoadLayout();

    showMaximized();
}

// ------------------ UI Setup ------------------

void EditorWindow::SetupMenuBar() {
    QMenu* fileMenu = menuBar()->addMenu("File");
    fileMenu->addAction("New Scene");
    fileMenu->addAction("Open...");
    fileMenu->addSeparator();
    fileMenu->addAction("Exit", this, &QWidget::close);

    QMenu* editMenu = menuBar()->addMenu("Edit");
    editMenu->addAction("Undo");
    editMenu->addAction("Redo");
}

void EditorWindow::SetupToolBar() {
    QToolBar* toolbar = addToolBar("Main Toolbar");
    toolbar->addAction("Play");
    toolbar->addAction("Pause");
    toolbar->addAction("Stop");
}

void EditorWindow::SetupStatusBar() {
    statusBar()->showMessage("Ready");
}
 
void EditorWindow::SetupDocks() {
    QWidget* sceneView = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(sceneView);
    QLabel* sceneLabel = new QLabel("Scene View Here");
    layout->addWidget(sceneLabel);
    sceneView->setLayout(layout);
    setCentralWidget(sceneView);
      // Enable flexible docking behavior
    setDockOptions(
    QMainWindow::AllowTabbedDocks |
    QMainWindow::AllowNestedDocks |
    QMainWindow::AnimatedDocks
    );
    setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::North);

    QDockWidget* hierarchyDock = new QDockWidget("Hierarchy", this);
    hierarchyDock->setObjectName("HierarchyDock");
    QTreeWidget* hierarchyTree = new QTreeWidget();
    hierarchyTree->setHeaderHidden(true);
    hierarchyDock->setWidget(hierarchyTree);
    addDockWidget(Qt::LeftDockWidgetArea, hierarchyDock);

    QDockWidget* inspectorDock = new QDockWidget("Inspector", this);
    inspectorDock->setObjectName("InspectorDock"); 
    QTextEdit* inspectorText = new QTextEdit();
    inspectorDock->setWidget(inspectorText);
    addDockWidget(Qt::RightDockWidgetArea, inspectorDock);

    QDockWidget* consoleDock = new QDockWidget("Console", this);
    consoleDock->setObjectName("ConsoleDock"); // 🔹
    QTextEdit* consoleText = new QTextEdit();
    consoleDock->setWidget(consoleText);
    addDockWidget(Qt::BottomDockWidgetArea, consoleDock);

    tabifyDockWidget(consoleDock, inspectorDock);
}

void EditorWindow::closeEvent(QCloseEvent* event) {
    SaveLayout();
    QMainWindow::closeEvent(event);
}


void EditorWindow::SaveLayout() {
    QSettings settings("Rocklyn", "RockEngineEditor");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
}

void EditorWindow::LoadLayout() {
    QSettings settings("Rocklyn", "RockEngineEditor");
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
}
