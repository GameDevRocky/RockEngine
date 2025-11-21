#include "FolderViewGui.hpp"
#include <QVBoxLayout>
#include <QLabel>
#include <QFileInfo>
#include "EditorUtils.hpp"

FolderViewGui::FolderViewGui(QWidget* parent) : QWidget(parent), currentPath("") {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // Create file system model
    model = new QFileSystemModel(this);
    model->setRootPath("");
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

    gridView->setDragEnabled(true);                    
    gridView->setAcceptDrops(false);                  
    gridView->setDropIndicatorShown(false);            
    gridView->setDragDropMode(QAbstractItemView::DragOnly); 
    gridView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    gridView->setSelectionMode(QAbstractItemView::SingleSelection);

    layout->addWidget(gridView);
    setLayout(layout);

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

void FolderViewGui::Navigate(const std::string& filepath) {
    QString path = QString::fromStdString(filepath);
    
    // Check if it's a directory
    QFileInfo fileInfo(path);
    if (!fileInfo.isDir())
        path = fileInfo.dir().absolutePath();
    
    // Set the root index to display contents
    currentPath = path;
    QModelIndex rootIndex = model->index(path);
    gridView->setRootIndex(rootIndex);
}