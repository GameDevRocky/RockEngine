#include "dock-widgets/FolderViewGui.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFileInfo>
#include <QWidget>
#include <QDir>
#include <QMenu>
#include <QMessageBox>
#include <QShortcut>
#include <QFile>
#include <fstream>
#include "utils/EditorUtils.hpp"
#include "utils/AssetPreviewDelegate.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/core/SelectionManager.hpp"
#include "engine/core/SceneManager.hpp"
#include "engine/core/Container.hpp"
#include "Engine.hpp"
#include "iostream"
#include <yaml-cpp/yaml.h>

namespace {
// Image extensions that can be imported (copied in + auto-serialized) by drop.
bool IsImportableImage(const QString& path) {
    const QString ext = QFileInfo(path).suffix().toLower();
    return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp";
}
}

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
    // Drops are handled at the FolderViewGui level (see dropEvent), not by the
    // grid's file-system model, so external files route to one place. Keeping
    // this false also stops the model from silently moving dropped files.
    gridView->setAcceptDrops(false);
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

    // Right-click anywhere in the grid opens the "New" asset menu.
    gridView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(gridView, &QListView::customContextMenuRequested, this, [this](const QPoint& pos) {
        ShowContextMenu(pos);
    });

    // Del key deletes the selected asset(s) (with a confirmation prompt). Scoped
    // to the grid so it only fires while the Folder view has focus.
    auto* deleteShortcut = new QShortcut(QKeySequence::Delete, gridView);
    deleteShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(deleteShortcut, &QShortcut::activated, this, [this]() { DeleteSelectedAssets(); });
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

// Accept a drag if it carries at least one file we know how to handle: an image
// to import (copy in + serialize) or a .material to (re)load.
void FolderViewGui::dragEnterEvent(QDragEnterEvent* event) {
    if (!event->mimeData()->hasUrls()) { event->ignore(); return; }
    for (const QUrl& url : event->mimeData()->urls()) {
        const QString path = url.toLocalFile();
        if (IsImportableImage(path) || path.endsWith(".material", Qt::CaseInsensitive)) {
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
    bool handled = false;
    for (const QUrl& url : event->mimeData()->urls()) {
        const QString path = url.toLocalFile();
        if (IsImportableImage(path)) {
            // Copy into the current folder, generate its .texture meta, and
            // register the texture — all via the existing asset systems.
            AssetManager::Get().ImportTexture(path.toStdString(), currentPath.toStdString());
            handled = true;
        } else if (path.endsWith(".material", Qt::CaseInsensitive)) {
            AssetManager::Get().LoadAssetFromFile(path.toStdString());
            handled = true;
        }
    }
    if (handled) event->acceptProposedAction();
    else         event->ignore();
}

void FolderViewGui::ShowContextMenu(const QPoint& pos) {
    // Right-clicking an item that isn't already selected makes it the target, so
    // "Delete" acts on what the user clicked (matching typical file managers).
    const QModelIndex idx = gridView->indexAt(pos);
    if (idx.isValid() && !gridView->selectionModel()->isSelected(idx))
        gridView->setCurrentIndex(idx);

    QMenu menu(this);
    QMenu* newMenu = menu.addMenu("New");
    newMenu->addAction("Material", this, [this]() { CreateNewMaterial(); });
    newMenu->addAction("Scene",    this, [this]() { CreateNewScene(); });

    const auto selected = GetSelectedFilePaths();
    if (!selected.empty()) {
        menu.addSeparator();
        const QString label = selected.size() == 1
            ? QString("Delete \"%1\"").arg(QFileInfo(QString::fromStdString(selected.front())).fileName())
            : QString("Delete %1 Items").arg(selected.size());
        menu.addAction(label, this, [this]() { DeleteSelectedAssets(); });
    }

    menu.exec(gridView->viewport()->mapToGlobal(pos));
}

void FolderViewGui::DeleteSelectedAssets() {
    const auto paths = GetSelectedFilePaths();
    if (paths.empty()) return;

    const QString message = (paths.size() == 1)
        ? QString("Are you sure you want to delete \"%1\"?\n\nThis action cannot be undone.")
              .arg(QFileInfo(QString::fromStdString(paths.front())).fileName())
        : QString("Are you sure you want to delete these %1 items?\n\nThis action cannot be undone.")
              .arg(paths.size());

    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle("Delete Asset");
    box.setText(message);
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setDefaultButton(QMessageBox::No);
    if (box.exec() != QMessageBox::Yes) return;

    for (const std::string& p : paths)
        DeleteOneAsset(QString::fromStdString(p));
    // QFileSystemModel watches the directory and drops the removed rows itself.
}

void FolderViewGui::DeleteOneAsset(const QString& filePath) {
    // Unregister an asset file from AssetManager memory by reading its id.
    auto unregisterByFile = [](const QString& f) {
        try {
            YAML::Node node = YAML::LoadFile(f.toStdString());
            if (node["id"]) AssetManager::Get().RemoveAsset(node["id"].as<std::string>());
        } catch (...) {}
    };

    const QString lower = filePath.toLower();
    QStringList toDelete;   // every file to remove from disk

    if (lower.endsWith(".texture") || lower.endsWith(".shader")) {
        // A meta file: unregister the asset, then delete the meta + its source
        // (foo.png.texture → foo.png, foo.glsl.shader → foo.glsl).
        unregisterByFile(filePath);
        toDelete << filePath;
        QString source = filePath;
        source.chop(QFileInfo(filePath).suffix().size() + 1);   // strip ".<metaExt>"
        if (QFileInfo::exists(source)) toDelete << source;
    } else if (lower.endsWith(".material") || lower.endsWith(".mat")) {
        unregisterByFile(filePath);
        toDelete << filePath;
    } else if (lower.endsWith(".scene")) {
        // A scene isn't an AssetManager asset -- it's loaded into a Container by
        // SceneManager. If this file is currently loaded, unload it (which clears
        // the hierarchy via REMOVED_SCENE_EVENT) before deleting the file. Remove
        // from both containers so it's gone in edit mode and any live play copy.
        try {
            YAML::Node node = YAML::LoadFile(filePath.toStdString());
            if (node["id"]) {
                const std::string sceneId = node["id"].as<std::string>();
                for (Container* c : { Engine::Get()->GetEditorContainer(),
                                      Engine::Get()->GetRuntimeContainer() }) {
                    if (!c) continue;
                    if (auto* sm = c->FindSystem<SceneManager>()) sm->RemoveScene(sceneId);
                }
            }
        } catch (...) {}
        toDelete << filePath;
    } else {
        // A source/definition file with no meta ext of its own (image without a
        // generated meta, a .scene, etc.). If it has a sibling meta, unregister
        // and delete that too so we don't leave an orphaned descriptor.
        for (const QString& metaExt : { QStringLiteral(".texture"), QStringLiteral(".shader") }) {
            const QString meta = filePath + metaExt;
            if (QFileInfo::exists(meta)) { unregisterByFile(meta); toDelete << meta; }
        }
        toDelete << filePath;
    }

    for (const QString& f : toDelete) {
        if (!QFile::remove(f))
            std::cerr << "[FolderViewGui] Failed to delete: " << f.toStdString() << std::endl;
    }
}

QString FolderViewGui::UniqueAssetPath(const QString& baseName, const QString& ext) const {
    QDir dir(currentPath);
    QString candidate = baseName + ext;
    for (int n = 1; QFileInfo::exists(dir.filePath(candidate)); ++n)
        candidate = QString("%1 %2%3").arg(baseName).arg(n).arg(ext);
    return dir.filePath(candidate);
}

void FolderViewGui::CreateNewMaterial() {
    const QString path = UniqueAssetPath("New Material", ".material");
    const std::string name = QFileInfo(path).completeBaseName().toStdString();
    AssetManager::Get().CreateMaterial(path.toStdString(), name);
    // QFileSystemModel watches the directory and shows the new file itself.
}

void FolderViewGui::CreateNewScene() {
    const QString path = UniqueAssetPath("New Scene", ".scene");

    // A minimal, valid empty scene: name + id + empty object/component lists.
    YAML::Node node;
    node["name"] = QFileInfo(path).completeBaseName().toStdString();
    node["id"]   = EngineUtils::GenerateUUID();
    node["gameobjects"] = YAML::Node(YAML::NodeType::Sequence);
    node["components"]  = YAML::Node(YAML::NodeType::Sequence);

    std::ofstream out(path.toStdString());
    if (!out.is_open()) {
        std::cerr << "[FolderViewGui] Failed to create scene: " << path.toStdString() << std::endl;
        return;
    }
    out << node;
}