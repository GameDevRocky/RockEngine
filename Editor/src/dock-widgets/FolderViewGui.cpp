#include "dock-widgets/FolderViewGui.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFileInfo>
#include <QWidget>
#include <QDir>
#include "utils/EditorUtils.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "iostream"

FolderViewGui::FolderViewGui(QWidget* parent) : QWidget(parent), currentPath(PROJECT_ROOT), projectDirectory(PROJECT_ROOT) {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    navBar = new QWidget(this);
    navBar->setFixedHeight(36);
    navBar->setStyleSheet("background-color: #2d2d2d; border-bottom: 1px solid #454545;");
    QHBoxLayout* navLayout = new QHBoxLayout(navBar);
    navLayout->setContentsMargins(8, 1, 8, 1);
    navLayout->setSpacing(4);

    breadcrumbLayout = new QHBoxLayout();
    breadcrumbLayout->setContentsMargins(0, 0, 0, 0);
    breadcrumbLayout->setSpacing(2);

    navLayout->addLayout(breadcrumbLayout, 1);
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
    std::cout << "FolderViewGui Initialized" << std::endl;
    this->SetProjectDirectory(EngineUtils::GetAssetPath("Domain/sandbox"));
}

void FolderViewGui::SetProjectDirectory(const std::string& projectDir) {
    projectDirectory = QString::fromStdString(projectDir);
    directoryHistory.clear();
    NavigateToPath(projectDirectory, false);
}

void FolderViewGui::Navigate(const std::string& filepath) {
    QString path = QString::fromStdString(filepath);
    
    // Check if it's a directory
    QFileInfo fileInfo(path);
    if (!fileInfo.isDir())
        path = fileInfo.dir().absolutePath();

    NavigateToPath(path, true);
}

void FolderViewGui::GoBack() {
    if (directoryHistory.isEmpty())
        return;

    QString previousPath = directoryHistory.pop();
    NavigateToPath(previousPath, false);
}

void FolderViewGui::NavigateToPath(const QString& path, bool recordHistory) {
    if (path.isEmpty()) return;

    QString normalizedPath = QDir(path).absolutePath();
    if (recordHistory && normalizedPath != currentPath && !currentPath.isEmpty()) {
        directoryHistory.push(currentPath);
    }

    currentPath = normalizedPath;
    QModelIndex rootIndex = model->index(currentPath);
    gridView->setRootIndex(rootIndex);
    RefreshBreadcrumbs();
}

QString FolderViewGui::RelativePathForBreadcrumb(const QString& path) const {
    QDir projectDir(projectDirectory);
    QString relative = projectDir.relativeFilePath(path);
    if (relative == ".") return QString();
    return relative;
}

void FolderViewGui::RefreshBreadcrumbs() {
    if (!breadcrumbLayout) return;

    while (QLayoutItem* item = breadcrumbLayout->takeAt(0)) {
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }

    QPushButton* rootButton = new QPushButton(QFileInfo(projectDirectory).fileName(), this);
    rootButton->setFlat(true);
    rootButton->setCursor(Qt::PointingHandCursor);
    rootButton->setStyleSheet("QPushButton { color: #e0e0e0; background: transparent; border: none; padding: 4px 6px; } QPushButton:hover { color: #ffffff; }");
    connect(rootButton, &QPushButton::clicked, this, [this]() {
        NavigateToPath(projectDirectory, true);
    });
    breadcrumbLayout->addWidget(rootButton);

    QString relativePath = RelativePathForBreadcrumb(currentPath);
    if (!relativePath.isEmpty()) {
        const QStringList parts = relativePath.split('/', Qt::SkipEmptyParts);
        QString accumulated = projectDirectory;

        for (const QString& part : parts) {
            QLabel* separator = new QLabel("/", this);
            separator->setStyleSheet("color: #8a8a8a;");
            breadcrumbLayout->addWidget(separator);

            accumulated = QDir(accumulated).filePath(part);

            QPushButton* crumbButton = new QPushButton(part, this);
            crumbButton->setFlat(true);
            crumbButton->setCursor(Qt::PointingHandCursor);
            crumbButton->setStyleSheet("QPushButton { color: #c9c9c9; background: transparent; border: none; padding: 4px 6px; } QPushButton:hover { color: #ffffff; }");

            const QString targetPath = QDir(accumulated).absolutePath();
            connect(crumbButton, &QPushButton::clicked, this, [this, targetPath]() {
                NavigateToPath(targetPath, true);
            });

            breadcrumbLayout->addWidget(crumbButton);
        }
    }

    breadcrumbLayout->addStretch();
}