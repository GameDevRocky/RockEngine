#include "dock-widgets/FolderViewGui.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFileInfo>
#include <QWidget>
#include <QDir>
#include "utils/EditorUtils.hpp"
#include "utils/AssetPreviewDelegate.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/core/SelectionManager.hpp"
#include "Engine.hpp"
#include "iostream"
#include <yaml-cpp/yaml.h>

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

    // Proxy model: filters compound-asset secondaries (.frag, .meta)
    proxy = new AssetFilterProxyModel(model, this);
    proxy->setSourceModel(model);

    gridView = new QListView(this);
    gridView->setViewMode(QListView::IconMode);
    gridView->setMovement(QListView::Static);
    gridView->setResizeMode(QListView::Adjust);
    gridView->setSpacing(8);
    gridView->setIconSize(QSize(64, 64));
    gridView->setWrapping(true);
    gridView->setUniformItemSizes(true);
    gridView->setGridSize(QSize(100, 110));  // Fixed cell size: 100px wide, 110px tall
    gridView->setTextElideMode(Qt::ElideRight);  // Add "..." to long filenames
    this->setAcceptDrops(true);
    gridView->setDragEnabled(true);                    
    gridView->setAcceptDrops(true);                  
    gridView->setDropIndicatorShown(false);            
    gridView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    gridView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    gridView->setItemDelegate(new AssetPreviewDelegate(model, gridView, this));

    gridView->setModel(proxy);

    mainLayout->addWidget(gridView);
    setLayout(mainLayout);
    setAcceptDrops(true);

    connect(gridView, &QListView::clicked, this, [this](const QModelIndex& proxyIndex) {
        QModelIndex sourceIndex = proxy->mapToSource(proxyIndex);
        if (model->isDir(sourceIndex)) return;

        const QString filePath = model->filePath(sourceIndex);
        const QString ext = QFileInfo(filePath).suffix().toLower();

        static const QSet<QString> assetExts = { "mat", "material", "texture", "shader" };
        if (assetExts.contains(ext)) {
            try {
                YAML::Node node = YAML::LoadFile(filePath.toStdString());
                if (node["id"]) {
                    std::string assetId = node["id"].as<std::string>();
                    auto* selMgr = Engine::Get()->GetActiveContainer()->FindSystem<SelectionManager>();
                    if (selMgr) selMgr->Select(assetId);
                }
            } catch (...) {}
        }
    });

    connect(gridView, &QListView::doubleClicked, this, [this](const QModelIndex& proxyIndex) {
        QModelIndex sourceIndex = proxy->mapToSource(proxyIndex);
        if (model->isDir(sourceIndex)) {
            Navigate(model->filePath(sourceIndex).toStdString());
        } else {
            EditorUtils::OpenInVSCode(model->filePath(sourceIndex).toStdString());
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
    QModelIndex sourceRoot = model->index(currentPath);
    gridView->setRootIndex(proxy->mapFromSource(sourceRoot));
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

std::vector<std::string> FolderViewGui::GetSelectedFilePaths() const {
    std::vector<std::string> paths;
    const auto indexes = gridView->selectionModel()->selectedIndexes();
    for (const QModelIndex& proxyIndex : indexes) {
        QModelIndex sourceIndex = proxy->mapToSource(proxyIndex);
        if (!model->isDir(sourceIndex))
            paths.push_back(model->filePath(sourceIndex).toStdString());
    }
    return paths;
}

void FolderViewGui::dragEnterEvent(QDragEnterEvent* event) {
    for (const QUrl& url : event->mimeData()->urls()) {
        if (url.toLocalFile().endsWith(".material", Qt::CaseInsensitive)) {
            event->acceptProposedAction();
            return;
        }
    }
    event->ignore();
}

void FolderViewGui::dragMoveEvent(QDragMoveEvent* event) {
    event->acceptProposedAction();
}

void FolderViewGui::dropEvent(QDropEvent* event) {
    for (const QUrl& url : event->mimeData()->urls()) {
        QString path = url.toLocalFile();
        if (path.endsWith(".material", Qt::CaseInsensitive)) {
            AssetManager::Get().LoadAssetFromFile(path.toStdString());
            event->acceptProposedAction();
        }
    }
}