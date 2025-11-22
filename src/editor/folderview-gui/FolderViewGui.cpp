#include "FolderViewGui.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFileInfo>
#include <QWidget>
#include "EditorUtils.hpp"

FolderViewGui::FolderViewGui(QWidget* parent) : QWidget(parent), currentPath("C:/Users/rockl/Coding Projects/RockEngine"), projectDirectory("C:/Users/rockl/Coding Projects/RockEngine") {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Create navigation bar (fixed height)
    QWidget* navBar = new QWidget(this);
    navBar->setFixedHeight(36);
    navBar->setStyleSheet("background-color: #2d2d2d; border-bottom: 1px solid #454545;");
    QHBoxLayout* navLayout = new QHBoxLayout(navBar);
    navLayout->setContentsMargins(1,1,1,1);
    navLayout->setSpacing(4);

    // Back button with arrow
    backButton = new QPushButton("◀", this);
    backButton->setFixedSize(28, 28);
    backButton->setFlat(true);
    backButton->setToolTip("Go to parent directory");
    backButton->setEnabled(false);

    connect(backButton, &QPushButton::clicked, this, &FolderViewGui::GoBack);

    navLayout->addWidget(backButton);
    navLayout->addStretch();
    navBar->setLayout(navLayout);

    mainLayout->addWidget(navBar);

    // Create file system model
    model = new QFileSystemModel(this);
    model->setRootPath(projectDirectory);
    model->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);
    model->setIconProvider(new EditorUtils::CustomIconProvider());
    gridView = new QListView(this);
    gridView->setModel(model);
    gridView->setViewMode(QListView::IconMode);
    gridView->setResizeMode(QListView::Adjust);
    gridView->setSpacing(8);
    gridView->setIconSize(QSize(64, 64));
    gridView->setWrapping(true);
    gridView->setUniformItemSizes(true);
    gridView->setGridSize(QSize(100, 110));  // Fixed cell size: 100px wide, 110px tall
    gridView->setTextElideMode(Qt::ElideRight);  // Add "..." to long filenames

    gridView->setDragEnabled(true);                    
    gridView->setAcceptDrops(false);                  
    gridView->setDropIndicatorShown(false);            
    gridView->setDragDropMode(QAbstractItemView::DragOnly); 
    gridView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    gridView->setSelectionMode(QAbstractItemView::SingleSelection);

    mainLayout->addWidget(gridView);
    setLayout(mainLayout);

    connect(gridView, &QListView::doubleClicked, this, [this](const QModelIndex& index) {
        if (model->isDir(index)) {
            QString folderPath = model->filePath(index);
            Navigate(folderPath.toStdString());
        }
        else{
            EditorUtils::OpenInVSCode(model->filePath(index).toStdString());
        }
    });
}

void FolderViewGui::Init() {
    setMinimumWidth(400);
}

void FolderViewGui::SetProjectDirectory(const std::string& projectDir) {
    projectDirectory = QString::fromStdString(projectDir);
    directoryHistory.clear();
    directoryHistory.push(projectDirectory);
    currentPath = projectDirectory;
    QModelIndex rootIndex = model->index(projectDirectory);
    gridView->setRootIndex(rootIndex);
    UpdateBackButtonState();
}

void FolderViewGui::Navigate(const std::string& filepath) {
    QString path = QString::fromStdString(filepath);
    
    // Check if it's a directory
    QFileInfo fileInfo(path);
    if (!fileInfo.isDir())
        path = fileInfo.dir().absolutePath();
    
    // Add to history if different from current
    if (path != currentPath) {
        directoryHistory.push(currentPath);
        currentPath = path;
    }
    
    QModelIndex rootIndex = model->index(path);
    gridView->setRootIndex(rootIndex);
    UpdateBackButtonState();
}

void FolderViewGui::GoBack() {
    if (directoryHistory.isEmpty())
        return;

    QString previousPath = directoryHistory.pop();
    currentPath = previousPath;
    
    QModelIndex rootIndex = model->index(previousPath);
    gridView->setRootIndex(rootIndex);
    UpdateBackButtonState();
}

void FolderViewGui::UpdateBackButtonState() {
    // Disable back button when at project directory
    if (!projectDirectory.isEmpty()) {
        backButton->setEnabled(currentPath != projectDirectory && !directoryHistory.isEmpty());
    } else {
        backButton->setEnabled(!directoryHistory.isEmpty());
    }
}