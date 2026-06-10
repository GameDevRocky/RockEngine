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
    explicit AssetPreviewDelegate(QFileSystemModel* fsModel,
                                   QAbstractItemView* view,
                                   QObject* parent = nullptr);
    ~AssetPreviewDelegate() override;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

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

    static constexpr int kThumbSize = 64;
};
