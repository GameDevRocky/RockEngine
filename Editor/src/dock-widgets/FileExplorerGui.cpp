#include "dock-widgets/FileExplorerGui.hpp"
#include <QVBoxLayout>
#include "dock-widgets/FolderViewGui.hpp"
#include "engine/debug/Console.hpp"
#include "utils/EditorUtils.hpp"


FileExplorerGui::FileExplorerGui(QWidget *parent)
    : QWidget(parent)
{
    // Layout for the entire widget
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    model = new QFileSystemModel(this);
    model->setRootPath(""); // will be set later
    model->setFilter(QDir::NoDotAndDotDot | QDir::AllDirs);
    model->setIconProvider(new EditorUtils::CustomIconProvider());
    
    
    tree = new QTreeView(this);
    tree->setModel(model);
    tree->setHeaderHidden(true);
    tree->setAnimated(true);
    tree->setIndentation(20);
    tree->setColumnHidden(1, true);  
    tree->setColumnHidden(2, true);  
    tree->setColumnHidden(3, true);
    
    tree->viewport()->setAutoFillBackground(true);
    tree->viewport()->setPalette(this->palette());
    layout->addWidget(tree);

    // When user selects a file/folder
    connect(tree, &QTreeView::clicked, [this](const QModelIndex& index) {
        FolderViewGui::Get()->Navigate(model->filePath(index).toStdString());
        emit RaiseFolderView();
        Console::Comment(model->filePath(index).toStdString());
    });

    // When user double-clicks a file/folder
    connect(tree, &QTreeView::doubleClicked, [this](const QModelIndex& index) {
        FolderViewGui::Get()->Navigate(model->filePath(index).toStdString());
        emit RaiseFolderView();
        Console::Comment(model->filePath(index).toStdString());
    });

    setLayout(layout);
}

void FileExplorerGui::SetProjectDirectory(const QString& dir)
{
    model->setRootPath(dir);
    tree->setRootIndex(model->index(dir));
}

void FileExplorerGui::mousePressEvent(QMouseEvent *event)
{
    QWidget::mousePressEvent(event);
}

void FileExplorerGui::Init(){
    setMinimumWidth(300);   
    setMaximumWidth(300);
    std::cout << "FileExplorerGui Initialized" << std::endl;

}