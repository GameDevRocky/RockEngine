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
#include <QLineEdit>
#include <QSlider>
#include <QKeyEvent>
#include <QItemSelectionModel>
#include <fstream>
#include "utils/EditorUtils.hpp"
#include "utils/AssetPreviewDelegate.hpp"
#include "utils/ProjectFileRenamer.hpp"
#include "utils/FolderAssetModel.hpp"
#include "utils/FolderAssetView.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/core/SelectionManager.hpp"
#include "engine/core/SceneManager.hpp"
#include "engine/core/Container.hpp"
#include "Engine.hpp"
#include "iostream"
#include <algorithm>
#include <QTimer>
#include <QEvent>
#include <array>
#include <yaml-cpp/yaml.h>
#include "engine/rendering/core/AssetMetaService.hpp"

namespace {
// The starter masks mirror Terrain Tileset's neighbor patterns. -1 means Any;
// 0 means empty and 1 means the starter tile itself.
constexpr std::array<std::array<int, 8>, 10> kDefaultTileSetNeighborMasks = {{
    {{ 1, -1,  0, -1,  0, -1,  1,  1}},
    {{-1, -1, -1, -1,  0,  0,  0, -1}},
    {{ 0, -1,  1, -1,  1,  1,  1, -1}},
    {{ 1,  1,  1,  1,  1, -1,  0, -1}},
    {{ 1, -1,  0, -1,  1,  1,  1,  1}},
    {{ 0,  0,  0, -1,  1,  1,  1, -1}},
    {{ 0, -1,  1,  1,  1, -1,  0,  0}},
    {{ 1,  0,  1,  1,  1,  1,  1,  1}},
    {{ 1,  1,  1,  1,  1,  1,  1,  0}},
    {{ 1,  1,  1, -1,  0, -1,  1,  1}}
}};

// Can this file be imported (copied in + auto-serialized) by dropping it here?
//
// Asked of AssetMetaService rather than answered with a list of its own: that
// table already decides what gets a meta on startup, and a second copy here is
// how images ended up droppable while audio, fonts and shaders -- all of which
// the meta service and AssetManager have always handled -- silently were not.
// Teaching MetaExtensionFor about a new type now makes it droppable too.
bool IsImportableAsset(const QString& path) {
    const std::string ext = "." + QFileInfo(path).suffix().toLower().toStdString();
    return !AssetMetaService::MetaExtensionFor(ext).empty();
}
}

FolderViewGui::FolderViewGui(QWidget* parent) : QWidget(parent), currentPath(PROJECT_ROOT), projectDirectory(PROJECT_ROOT) {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    navBar = new QWidget(this);
    navBar->setFixedHeight(24);
    navBar->setStyleSheet("background: transparent; border-bottom: 1px solid #2f2f2f;");
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

    m_entries = new FolderAssetModel(proxy, this);
    gridView = new FolderAssetView(this);
    gridView->setViewMode(QListView::IconMode);
    gridView->setMovement(QListView::Static);
    gridView->setResizeMode(QListView::Adjust);
    gridView->setSpacing(4);
    gridView->setIconSize(QSize(AssetPreviewDelegate::kThumbSize, AssetPreviewDelegate::kThumbSize));
    gridView->setWrapping(true);
    gridView->setUniformItemSizes(true);
    gridView->setGridSize(QSize(AssetPreviewDelegate::kCellWidth,
                               AssetPreviewDelegate::kCellHeight));
    gridView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
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
    m_delegate = new AssetPreviewDelegate(gridView, this);
    gridView->setItemDelegate(m_delegate);

    static_cast<FolderAssetView*>(gridView)->SetAssetModel(m_entries);

    mainLayout->addWidget(gridView, 1);

    auto* footer = new QWidget(this);
    auto* footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(8, 2, 8, 2);
    footerLayout->setSpacing(6);
    footerLayout->addStretch();
    footerLayout->addWidget(new QLabel("Size", footer));
    auto* cellSizeSlider = new QSlider(Qt::Horizontal, footer);
    cellSizeSlider->setObjectName("FolderCellSizeSlider");
    cellSizeSlider->setRange(24, 128);
    cellSizeSlider->setValue(AssetPreviewDelegate::kThumbSize);
    cellSizeSlider->setFixedWidth(120);
    cellSizeSlider->setToolTip("Adjust Folder View cell size");
    cellSizeSlider->setAccessibleName("Folder View cell size");
    footerLayout->addWidget(cellSizeSlider);
    mainLayout->addWidget(footer);
    connect(cellSizeSlider, &QSlider::valueChanged, this, [this](int size) {
        m_delegate->setThumbnailSize(size);
        gridView->setIconSize(QSize(size, size));
        gridView->setGridSize(m_delegate->cellSize());
        static_cast<FolderAssetView*>(gridView)->RefreshToggleButtons();
        gridView->viewport()->update();
    });
    setLayout(mainLayout);
    setAcceptDrops(true);

    connect(gridView, &QListView::clicked, this, [this](const QModelIndex& proxyIndex) {
        const QString spriteId = proxyIndex.data(FolderAssetModel::SpriteIdRole).toString();
        if (!spriteId.isEmpty()) {
            auto* selection = Engine::Get()->GetActiveContainer()->FindSystem<SelectionManager>();
            if (selection) selection->Select(spriteId.toStdString());
            return;
        }
        if (proxyIndex.data(FolderAssetModel::IsDirectoryRole).toBool()) return;
        const QString filePath = proxyIndex.data(FolderAssetModel::FilePathRole).toString();
        const QString ext = QFileInfo(filePath).suffix().toLower();

        // Every file-backed type AssetManager can load, so clicking one selects
        // the asset it describes in the Inspector.
        static const QSet<QString> assetExts = {
            "mat", "material", "texture", "shader", "font", "audio", "tilemap",
            "tileset", "scene" };
        if (assetExts.contains(ext)) {
            try {
                YAML::Node node = YAML::LoadFile(filePath.toStdString());
                if (node["id"]) {
                    std::string assetId = node["id"].as<std::string>();
                    auto* selMgr = Engine::Get()->GetActiveContainer()->FindSystem<SelectionManager>();
                    if (selMgr) selMgr->Select(assetId);
                }
            } catch (...) {
            }
        }
    });

    connect(gridView, &QListView::doubleClicked, this, [this](const QModelIndex& proxyIndex) {
        if (!proxyIndex.data(FolderAssetModel::SpriteIdRole).toString().isEmpty()) return;
        const QString path = proxyIndex.data(FolderAssetModel::FilePathRole).toString();
        if (proxyIndex.data(FolderAssetModel::IsDirectoryRole).toBool())
            Navigate(path.toStdString());
        else
            EditorUtils::OpenInVSCode(path.toStdString());
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
    connect(deleteShortcut, &QShortcut::activated, this, [this]() {
        if (!m_renameEditor) DeleteSelectedAssets();
    });

    auto* renameShortcut = new QShortcut(QKeySequence(Qt::Key_F2), gridView);
    renameShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(renameShortcut, &QShortcut::activated, this, [this]() {
        if (gridView->selectionModel()->selectedIndexes().size() == 1)
            BeginRename(gridView->currentIndex());
    });

    gridView->setMouseTracking(true);
    gridView->viewport()->setMouseTracking(true);
}

FolderViewGui::~FolderViewGui() = default;

void FolderViewGui::Init() {
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
    CancelRename();

    QString normalizedPath = QDir(path).absolutePath();
    if (recordHistory && normalizedPath != currentPath && !currentPath.isEmpty()) {
        directoryHistory.push(currentPath);
    }

    currentPath = normalizedPath;
    QModelIndex sourceRoot = model->index(currentPath);
    m_entries->SetDirectory(proxy->mapFromSource(sourceRoot));
    gridView->scrollToTop();
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
    for (const QModelIndex& index : gridView->selectionModel()->selectedIndexes()) {
        const QString path = index.data(FolderAssetModel::FilePathRole).toString();
        if (!path.isEmpty() && !index.data(FolderAssetModel::IsDirectoryRole).toBool())
            paths.push_back(path.toStdString());
    }
    return paths;
}

bool FolderViewGui::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_renameEditor && event->type() == QEvent::KeyPress &&
        static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
        CancelRename();
        return true;
    }
    return QWidget::eventFilter(obj, event);
}

// Accept a drag if it carries at least one file we know how to handle: any
// source asset to import (copy in + serialize) or a .material to (re)load.
void FolderViewGui::dragEnterEvent(QDragEnterEvent* event) {
    if (!event->mimeData()->hasUrls()) { event->ignore(); return; }
    for (const QUrl& url : event->mimeData()->urls()) {
        const QString path = url.toLocalFile();
        if (IsImportableAsset(path) || path.endsWith(".material", Qt::CaseInsensitive)) {
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
        if (IsImportableAsset(path)) {
            // Copy into the current folder, generate its meta, and register the
            // asset — all via the existing asset systems. Images, audio clips,
            // fonts and shaders all take this one path.
            AssetManager::Get().ImportAsset(path.toStdString(), currentPath.toStdString());
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
        gridView->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect);

    QMenu menu(this);
    QMenu* newMenu = menu.addMenu("New");
    newMenu->addAction("Material", this, [this]() { CreateNewMaterial(); });
    newMenu->addAction("Tile Map", this, [this]() { CreateNewTileMap(); });
    newMenu->addAction("Tile Set", this, [this]() { CreateNewTileSet(); });
    newMenu->addAction("Scene",    this, [this]() { CreateNewScene(); });
    newMenu->addAction("Script",   this, [this]() { CreateNewScript(); });

    const auto selected = GetSelectedFilePaths();
    QStringList selectedSprites;
    for (const QModelIndex& selectedIndex : gridView->selectionModel()->selectedIndexes()) {
        if (!selectedIndex.data(FolderAssetModel::SpriteIdRole).toString().isEmpty())
            selectedSprites.push_back(selectedIndex.data(Qt::DisplayRole).toString());
    }
    if (idx.isValid() && gridView->selectionModel()->selectedIndexes().size() == 1 &&
        ProjectFileRenamer::CanRename(idx.data(FolderAssetModel::FilePathRole).toString())) {
        menu.addSeparator();
        menu.addAction("Rename", this, [this, idx]() { BeginRename(idx); });
    }
    const int deleteCount = static_cast<int>(selected.size()) + selectedSprites.size();
    if (deleteCount > 0) {
        menu.addSeparator();
        const QString label = deleteCount == 1
            ? QString("Delete \"%1\"").arg(selectedSprites.isEmpty()
                ? QFileInfo(QString::fromStdString(selected.front())).fileName()
                : selectedSprites.front())
            : QString("Delete %1 Items").arg(deleteCount);
        menu.addAction(label, this, [this]() { DeleteSelectedAssets(); });
    }

    menu.exec(gridView->viewport()->mapToGlobal(pos));
}

void FolderViewGui::BeginRename(const QModelIndex& index) {
    if (!index.isValid()) return;
    const QString path = index.data(FolderAssetModel::FilePathRole).toString();
    if (path.isEmpty() || !ProjectFileRenamer::CanRename(path)) return;
    CancelRename();
    m_renamePath = path;
    m_renameEditor = new QLineEdit(gridView->viewport());
    m_renameEditor->setText(ProjectFileRenamer::EditableName(path));
    const QRect cell = gridView->visualRect(index);
    const int width = std::min(std::max(170, cell.width()),
                               gridView->viewport()->width());
    const int left = std::clamp(cell.left(), 0,
        std::max(0, gridView->viewport()->width() - width));
    m_renameEditor->setGeometry(left, cell.bottom() - AssetPreviewDelegate::kLabelHeight,
                                width, AssetPreviewDelegate::kLabelHeight + 2);
    m_renameEditor->installEventFilter(this);
    connect(m_renameEditor, &QLineEdit::returnPressed, this, &FolderViewGui::CommitRename);
    connect(m_renameEditor, &QLineEdit::editingFinished, this, &FolderViewGui::CommitRename);
    m_renameEditor->show();
    m_renameEditor->setFocus();
    m_renameEditor->selectAll();
}

void FolderViewGui::CancelRename() {
    if (!m_renameEditor) return;
    QLineEdit* editor = m_renameEditor;
    m_renameEditor = nullptr;
    m_renamePath.clear();
    editor->removeEventFilter(this);
    editor->deleteLater();
}

void FolderViewGui::CommitRename() {
    if (!m_renameEditor) return;
    const QString path = m_renamePath;
    const QString name = m_renameEditor->text();
    CancelRename();
    QString error;
    if (!ProjectFileRenamer::Rename(path, name, &error)) {
        auto* message = new QMessageBox(QMessageBox::Warning, "Rename Failed", error,
                                        QMessageBox::Ok, this);
        message->setAttribute(Qt::WA_DeleteOnClose);
        message->open();
    }
}

void FolderViewGui::DeleteSelectedAssets() {
    const auto paths = GetSelectedFilePaths();
    std::vector<std::string> spriteIds;
    QStringList spriteNames;
    for (const QModelIndex& index : gridView->selectionModel()->selectedIndexes()) {
        const QString spriteId = index.data(FolderAssetModel::SpriteIdRole).toString();
        if (spriteId.isEmpty()) continue;
        spriteIds.push_back(spriteId.toStdString());
        spriteNames.push_back(index.data(Qt::DisplayRole).toString());
    }
    const int deleteCount = static_cast<int>(paths.size()) + static_cast<int>(spriteIds.size());
    if (deleteCount == 0) return;

    const QString singleName = spriteNames.isEmpty()
        ? QFileInfo(QString::fromStdString(paths.front())).fileName()
        : spriteNames.front();
    const QString message = (deleteCount == 1)
        ? QString("Are you sure you want to delete \"%1\"?\n\nThis action cannot be undone.")
              .arg(singleName)
        : QString("Are you sure you want to delete these %1 items?\n\nThis action cannot be undone.")
              .arg(deleteCount);

    auto* box = new QMessageBox(QMessageBox::Warning, "Delete Asset", message,
                                QMessageBox::Yes | QMessageBox::No, this);
    box->setDefaultButton(QMessageBox::No);
    box->setAttribute(Qt::WA_DeleteOnClose);
    connect(box, &QMessageBox::finished, this,
            [this, paths, spriteIds](int result) {
        if (result != QMessageBox::Yes) return;
        // Deleting a selected texture also removes its sprites. Resolve each ID
        // after file deletion so a selected child is never removed twice.
        for (const std::string& path : paths)
            DeleteOneAsset(QString::fromStdString(path));
        for (const std::string& id : spriteIds)
            if (AssetManager::Get().GetSprite(id)) AssetManager::Get().RemoveSprite(id);
    });
    box->open();
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
    } else if (lower.endsWith(".material") || lower.endsWith(".mat") ||
               lower.endsWith(".tilemap") || lower.endsWith(".tileset")) {
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

QString FolderViewGui::UniquePathIn(const QString& folder, const QString& baseName,
                                   const QString& ext, const QString& separator) const {
    QDir dir(folder);
    QString candidate = baseName + ext;
    for (int n = 1; QFileInfo::exists(dir.filePath(candidate)); ++n)
        candidate = QString("%1%2%3%4").arg(baseName, separator).arg(n).arg(ext);
    return dir.filePath(candidate);
}

QString FolderViewGui::UniqueAssetPath(const QString& baseName, const QString& ext) const {
    return UniquePathIn(currentPath, baseName, ext);
}

void FolderViewGui::CreateNewMaterial() {
    const QString path = UniqueAssetPath("New Material", ".material");
    AssetManager::Get().CreateMaterial(path.toStdString());
    // QFileSystemModel watches the directory and shows the new file itself.
}

void FolderViewGui::CreateNewTileMap() {
    const QString path = UniqueAssetPath("New Tile Map", ".tilemap");

    YAML::Node node;
    node["type"]  = "TileMap";
    node["id"]    = EngineUtils::GenerateUUID();
    node["cells"] = YAML::Node(YAML::NodeType::Sequence);

    std::ofstream out(path.toStdString());
    if (!out.is_open()) {
        std::cerr << "[FolderViewGui] Failed to create tile map: "
                  << path.toStdString() << std::endl;
        return;
    }
    out << node;
    out.close();

    AssetManager::Get().LoadAssetFromFile(path.toStdString());
}

void FolderViewGui::CreateNewTileSet() {
    const QString path = UniqueAssetPath("New Tile Set", ".tileset");

    YAML::Node node;
    node["type"]      = "TileSet";
    node["id"]        = EngineUtils::GenerateUUID();

    // Start with the Terrain TileSet's neighbor masks, without sprite bindings.
    YAML::Node tiles(YAML::NodeType::Sequence);
    YAML::Node tile;
    tile["id"] = 1;
    tile["name"] = "Tile 1";
    tile["defaultSpriteId"] = "";

    YAML::Node rules(YAML::NodeType::Sequence);
    for (const auto& pattern : kDefaultTileSetNeighborMasks) {
        YAML::Node rule;
        YAML::Node neighborIds(YAML::NodeType::Sequence);
        for (int neighbor : pattern) {
            if (neighbor < 0) neighborIds.push_back("any");
            else neighborIds.push_back(neighbor);
        }
        neighborIds.SetStyle(YAML::EmitterStyle::Flow);
        rule["neighbors"] = neighborIds;
        rule["spriteId"] = "";
        rules.push_back(rule);
    }
    tile["rules"] = rules;
    tiles.push_back(tile);
    node["tiles"] = tiles;

    std::ofstream out(path.toStdString());
    if (!out.is_open()) {
        std::cerr << "[FolderViewGui] Failed to create tile set: "
                  << path.toStdString() << std::endl;
        return;
    }
    out << node;
    out.close();

    AssetManager::Get().LoadAssetFromFile(path.toStdString());
}

void FolderViewGui::CreateNewScene() {
    const QString path = UniqueAssetPath("New Scene", ".scene");

    // A minimal, valid empty scene: name + id + empty object/component lists.
    YAML::Node node;
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

void FolderViewGui::CreateNewScript() {
    // Scripts are always created in Domain/sandbox/scripts, wherever the browser
    // happens to be: that folder is the only one script discovery scans and the
    // only one on the interpreter's sys.path, so a .py written anywhere else
    // would be a file the engine can never attach. The view then navigates there
    // so the new file is visible and immediately renameable.
    const QString scriptsDir =
        QString::fromStdString(EngineUtils::GetAssetPath("Domain/sandbox/scripts"));
    if (!QDir().mkpath(scriptsDir)) {
        std::cerr << "[FolderViewGui] Failed to create scripts folder: "
                  << scriptsDir.toStdString() << std::endl;
        return;
    }

    // No separator before the counter, and no spaces in the base name: the stem
    // is imported as a Python module name, so it has to stay an identifier.
    const QString path = UniquePathIn(scriptsDir, "NewScript", ".py", "");
    const QString className = QFileInfo(path).completeBaseName();

    // Class named after the file — the one-class-per-file convention the script
    // picker and the .py drag-and-drop resolution both prefer (see
    // RefDropFilter::ScriptRefForFile).
    const QString source = QString(
        "from Domain import *\n"
        "\n"
        "\n"
        "class %1(ScriptableComponent):\n"
        "    \"\"\"A new RockEngine script. Annotated class attributes become\n"
        "    inspector fields, e.g. `speed: float = 5.0`.\n"
        "    \"\"\"\n"
        "\n"
        "    def start(self):\n"
        "        pass\n"
        "\n"
        "    def update(self):\n"
        "        pass\n").arg(className);

    std::ofstream out(path.toStdString());
    if (!out.is_open()) {
        std::cerr << "[FolderViewGui] Failed to create script: " << path.toStdString() << std::endl;
        return;
    }
    out << source.toStdString();
    out.close();

    // The file is discoverable as soon as it lands on disk — script_discovery
    // invalidates the import caches on every scan — so nothing needs registering
    // here, unlike materials and scenes.
    if (QDir(currentPath) != QDir(scriptsDir))
        Navigate(scriptsDir.toStdString());
}
