#pragma once
#include <vector>
#include <string>
#include <QWidget>
#include <QListView>
#include <QFileSystemModel>
#include <QPushButton>
#include <QString>
#include <QStack>
#include <QHBoxLayout>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QPixmap>
#include <QModelIndex>
#include <QRect>
#include <unordered_map>
#include <utility>
#include "utils/AssetFilterProxyModel.hpp"

class SpriteHoverColumn;
class AssetPreviewDelegate;
class QTimer;
class QVariantAnimation;

class FolderViewGui : public QWidget {
    Q_OBJECT

public:
    static FolderViewGui* Get() {
        static FolderViewGui* instance = nullptr;
        if (!instance) {
            instance = new FolderViewGui(nullptr);
        }
        return instance;
    }
    void Init();
    void Navigate(const std::string& filepath);
    void SetProjectDirectory(const std::string& projectDir);
    std::vector<std::string> GetSelectedFilePaths() const;

private:
    explicit FolderViewGui(QWidget* parent = nullptr);
    ~FolderViewGui() override;
    void GoBack();
    void RefreshBreadcrumbs();
    void NavigateToPath(const QString& path, bool recordHistory = true);
    QString RelativePathForBreadcrumb(const QString& path) const;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

    // Right-click "New" menu: create asset files on disk in the current folder.
    void ShowContextMenu(const QPoint& pos);
    void CreateNewMaterial();
    void CreateNewScene();
    void CreateNewScript();
    // A non-colliding path in `folder` for "<baseName><ext>", adding "<sep>1",
    // "<sep>2", … before the extension if needed. Scripts pass an empty separator
    // because the file stem is also a Python module name.
    QString UniquePathIn(const QString& folder, const QString& baseName,
                         const QString& ext, const QString& separator = " ") const;
    // The same, in the folder currently being browsed.
    QString UniqueAssetPath(const QString& baseName, const QString& ext) const;

    // Delete the currently selected asset(s) after a confirmation prompt. Removes
    // the file from disk (plus any companion source/meta) and the asset from
    // AssetManager memory. Triggered by the Del key or the context-menu action.
    void DeleteSelectedAssets();
    void DeleteOneAsset(const QString& filePath);

    // ── Sprite hover column ──────────────────────────────────────────────────
    // On hovering a .texture cell, show a scrollable column of its sprites over
    // the cell. Coordinated with a short hide timer so the mouse can travel from
    // the cell onto the (separate top-level) column without it vanishing.
    bool eventFilter(QObject* obj, QEvent* event) override;
    void UpdateSpriteHover(const QModelIndex& proxyIndex);
    void ScheduleHideSpriteColumn();
    void EnsureSpriteColumn();
    // Actually build + show the column for m_pendingTextureId over m_pendingCellRect.
    void ShowSpriteColumnNow();
    // Thumbnails + names + ids for every sprite bound to a texture id (rendered
    // once, then cached until the folder changes). The parallel vectors feed the
    // hover column, whose ids double as the drag payload.
    struct TextureSprites {
        std::vector<QPixmap>     thumbs;
        std::vector<QString>     names;
        std::vector<std::string> ids;
    };
    const TextureSprites& SpritesForTexture(const std::string& textureId);
    // Drop a texture's cached sprite previews (after an in-memory sprite add/remove)
    // so the next hover rebuilds them; refresh the column live if it's showing it.
    void InvalidateSpriteCache(const std::string& textureId);

    // Fade the given texture cell out (while its column is up) / back in, and
    // repaint the cell whose source file is `filePath`.
    void FadeTextureCellOut(const QString& filePath);
    void FadeTextureCellIn();
    void RepaintCell(const QString& filePath);

    SpriteHoverColumn*   m_spriteColumn = nullptr;
    AssetPreviewDelegate* m_delegate    = nullptr;
    QTimer*              m_hideTimer     = nullptr;
    QTimer*              m_showTimer     = nullptr;   // tooltip-style appear delay
    QVariantAnimation*   m_cellFadeAnim  = nullptr;   // texture cell fade out/in
    QString              m_fadedFilePath;             // texture cell currently faded
    double               m_fadeOpacityNow = 1.0;      // live fade value
    std::string          m_pendingTextureId;          // texture awaiting the show delay
    QString              m_pendingFilePath;           // its source file path
    QRect                m_pendingCellRect;           // global rect of the hovered cell
    std::unordered_map<std::string, TextureSprites> m_spriteCache;
    int m_assetAddedSub   = -1;   // AssetManager ASSET_ADDED_EVENT subscription
    int m_assetRemovedSub = -1;   // AssetManager ASSET_REMOVED_EVENT subscription

    QListView*              gridView          = nullptr;
    QFileSystemModel*       model             = nullptr;
    AssetFilterProxyModel*  proxy             = nullptr;
    QWidget*                navBar            = nullptr;
    QHBoxLayout*            breadcrumbLayout  = nullptr;
    QString                 currentPath;
    QString                 projectDirectory;
    QStack<QString>         directoryHistory;
};