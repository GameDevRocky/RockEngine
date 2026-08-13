#pragma once
#include <QStyledItemDelegate>
#include <QFileSystemModel>
#include <QAbstractItemView>
#include <QPixmap>
#include <glad/glad.h>
#include <unordered_map>
#include <vector>

class Resource;

class AssetPreviewDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    // ── Cell metrics: the single source of truth ────────────────────────────
    // Public because the delegate only *draws* a cell -- the view that hosts it
    // decides how big cells are (setIconSize/setGridSize), and SpriteHoverColumn
    // has to line its own cells up with them. All three used to hardcode the
    // same numbers separately, so changing the thumbnail size here left the grid
    // still reserving room for the old one.
    //
    // Height is derived from the layout drawCell() actually uses: the thumbnail
    // sits kThumbTopPad from the top, and the label starts kLabelGap below the
    // thumbnail's own height.
    static constexpr int kThumbSize   = 48;
    static constexpr int kThumbTopPad = 8;
    static constexpr int kLabelGap    = 12;
    static constexpr int kLabelHeight = 16;   // one elided line + breathing room
    static constexpr int kCellWidth   = 64;
    static constexpr int kCellHeight  = kThumbSize + kLabelGap + kLabelHeight;

    explicit AssetPreviewDelegate(QFileSystemModel* fsModel,
                                   QAbstractItemView* view,
                                   QObject* parent = nullptr);
    ~AssetPreviewDelegate() override;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

    // Draw the cell for `filePath` at `opacity` (0..1). Used by the Folder view to
    // fade a texture out while its sprite hover-column is shown. Empty path clears.
    void SetCellFade(const QString& filePath, double opacity);
    void ClearCellFade();

private:
    // Returns the absolute path for a (possibly proxy-wrapped) index.
    QString filePathForIndex(const QModelIndex& index) const;

    // Blocking: must be called with a valid GL context active.
    QPixmap renderMaterialPreview(const QString& matPath) const;

    // Lazy-init the shared quad VAO/VBO used for all GL previews.
    // Must be called inside makeCurrent().
    void ensureQuadGeometry() const;

    void drawCell(QPainter* p, const QStyleOptionViewItem& opt,
                  const QPixmap& thumb, const QString& label) const;

    void subscribeToAsset(Resource* asset);
    void refreshSubscriptions();
    void repaintAsset(Resource* asset);

    QFileSystemModel*    m_fsModel = nullptr;
    QAbstractItemView*   m_view    = nullptr;

    // GL resources for preview rendering (owned, created lazily in SceneView context)
    mutable GLuint m_quadVAO = 0;
    mutable GLuint m_quadVBO = 0;

    std::unordered_map<Resource*, std::vector<int>> m_subscriptions;
    int m_managerSubId = -1;

    // The single cell currently faded (its source file path) and its opacity.
    QString m_fadeFilePath;
    double  m_fadeOpacity = 1.0;
};
