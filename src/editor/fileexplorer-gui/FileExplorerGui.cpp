#include "FileExplorerGui.hpp"
#include <QVBoxLayout>

FileExplorerGui::FileExplorerGui(QWidget *parent)
    : QWidget(parent)
{
    // Layout for the entire widget
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    model = new QFileSystemModel(this);
    model->setRootPath(""); // will be set later
    model->setFilter(QDir::NoDotAndDotDot | QDir::AllDirs);


    tree = new QTreeView(this);
    tree->setModel(model);
    tree->setHeaderHidden(true);
    tree->setAnimated(true);
    tree->setIndentation(20);
    tree->setColumnHidden(1, true);  
    tree->setColumnHidden(2, true);  
    tree->setColumnHidden(3, true);

    layout->addWidget(tree);

    // When user selects a file/folder
    connect(tree, &QTreeView::clicked, [this](const QModelIndex& index) {
        emit FileSelected(model->filePath(index));
    });

    // When user double-clicks a file/folder
    connect(tree, &QTreeView::doubleClicked, [this](const QModelIndex& index) {
        emit FileDoubleClicked(model->filePath(index));
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

}