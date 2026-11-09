#pragma once

#include <QAbstractListModel>
#include <QPersistentModelIndex>
#include <QSet>
#include <QSize>
#include <QString>
#include <vector>

class AssetFilterProxyModel;

// Flattens the current directory and inserts each expanded texture's sprites
// immediately after it. Sprite rows are sub-assets, never filesystem entries.
class FolderAssetModel final : public QAbstractListModel {
public:
    enum Role {
        FilePathRole = Qt::UserRole + 1,
        AssetIdRole,
        TextureIdRole,
        SpriteIdRole,
        SpriteSizeRole,
        IsDirectoryRole,
        ExpandedRole,
        HasSpritesRole
    };

    explicit FolderAssetModel(AssetFilterProxyModel* source, QObject* parent = nullptr);
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QStringList mimeTypes() const override;
    QMimeData* mimeData(const QModelIndexList& indexes) const override;
    Qt::DropActions supportedDragActions() const override { return Qt::CopyAction; }

    void SetDirectory(const QModelIndex& sourceRoot);
    void ToggleExpanded(const QModelIndex& index);
    QModelIndex TextureIndex(const QString& textureId) const;

private:
    struct Entry {
        QPersistentModelIndex source;
        QString filePath;
        QString name;
        QString assetId;
        QString textureId;
        QString spriteId;
        QSize spriteSize;
        bool directory = false;
        bool expanded = false;
        bool hasSprites = false;
        QString Key() const { return spriteId.isEmpty() ? filePath : spriteId; }
    };

    void QueueRefresh();
    void Refresh();

    AssetFilterProxyModel* m_source;
    QPersistentModelIndex m_root;
    QSet<QString> m_expandedTextures;
    std::vector<Entry> m_entries;
    bool m_refreshQueued = false;
};
