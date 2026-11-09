#include "utils/FolderAssetModel.hpp"

#include <algorithm>
#include <cmath>
#include <QCollator>
#include <QFileInfo>
#include <QHash>
#include <QMimeData>
#include <QTimer>
#include <QUrl>
#include <yaml-cpp/yaml.h>

#include "engine/rendering/core/AssetManager.hpp"
#include "engine/rendering/core/Sprite.hpp"
#include "utils/AssetFilterProxyModel.hpp"
#include "utils/DragDropMime.hpp"
#include "utils/ObjectRefPickerObserver.hpp"

FolderAssetModel::FolderAssetModel(AssetFilterProxyModel* source, QObject* parent)
    : QAbstractListModel(parent), m_source(source)
{
    auto refresh = [this] { QueueRefresh(); };
    connect(source, &QAbstractItemModel::rowsInserted, this, refresh);
    connect(source, &QAbstractItemModel::rowsRemoved, this, refresh);
    connect(source, &QAbstractItemModel::dataChanged, this, refresh);
    connect(source, &QAbstractItemModel::layoutChanged, this, refresh);
    connect(source, &QAbstractItemModel::modelReset, this, refresh);

    for (auto tag : {Properties::Tags::SPRITE, Properties::Tags::TEXTURE,
                     Properties::Tags::MATERIAL, Properties::Tags::SHADER}) {
        const bool changesEntries = tag == Properties::Tags::SPRITE || tag == Properties::Tags::TEXTURE;
        auto* observer = new ObjectRefPickerObserver(
            Properties::PropDesc().Tag(tag), [this, changesEntries] {
                if (changesEntries) QueueRefresh();
                else if (rowCount())
                    emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {Qt::DecorationRole});
            }, this);
        observer->Start();
    }
}

int FolderAssetModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant FolderAssetModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) return {};
    const Entry& entry = m_entries[index.row()];
    switch (role) {
        case Qt::DisplayRole: return entry.name;
        case Qt::DecorationRole: return entry.source.data(role);
        case Qt::ToolTipRole:
            return entry.spriteId.isEmpty() ? entry.filePath
                : QString("%1\n%2 x %3 pixels").arg(entry.name)
                    .arg(entry.spriteSize.width()).arg(entry.spriteSize.height());
        case FilePathRole: return entry.filePath;
        case AssetIdRole: return entry.assetId;
        case TextureIdRole: return entry.textureId;
        case SpriteIdRole: return entry.spriteId;
        case SpriteSizeRole: return entry.spriteSize;
        case IsDirectoryRole: return entry.directory;
        case ExpandedRole: return entry.expanded;
        case HasSpritesRole: return entry.hasSprites;
        default: return {};
    }
}

Qt::ItemFlags FolderAssetModel::flags(const QModelIndex& index) const {
    return index.isValid() ? Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled
                           : Qt::NoItemFlags;
}

QStringList FolderAssetModel::mimeTypes() const {
    return {"text/uri-list", kSpriteMimeType, kSpriteListMimeType};
}

QMimeData* FolderAssetModel::mimeData(const QModelIndexList& indexes) const {
    auto* mime = new QMimeData();
    QModelIndexList ordered = indexes;
    std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) {
        return a.row() < b.row();
    });
    QStringList sprites;
    QList<QUrl> urls;
    for (const QModelIndex& index : ordered) {
        const QString sprite = index.data(SpriteIdRole).toString();
        if (!sprite.isEmpty()) {
            if (!sprites.contains(sprite)) sprites.push_back(sprite);
        } else {
            const QString path = index.data(FilePathRole).toString();
            const QUrl url = QUrl::fromLocalFile(path);
            if (!path.isEmpty() && !urls.contains(url)) urls.push_back(url);
        }
    }
    if (!urls.empty()) mime->setUrls(urls);
    if (!sprites.empty()) {
        mime->setData(kSpriteMimeType, sprites.front().toUtf8());
        mime->setData(kSpriteListMimeType, sprites.join('\n').toUtf8());
    }
    return mime;
}

void FolderAssetModel::SetDirectory(const QModelIndex& sourceRoot) {
    beginResetModel();
    m_root = sourceRoot;
    m_entries.clear();
    endResetModel();
    if (m_root.isValid() && m_source->canFetchMore(m_root)) m_source->fetchMore(m_root);
    Refresh();
}

void FolderAssetModel::ToggleExpanded(const QModelIndex& index) {
    if (!index.isValid() || !index.data(SpriteIdRole).toString().isEmpty() ||
        !index.data(HasSpritesRole).toBool()) return;
    const QString id = index.data(TextureIdRole).toString();
    if (m_expandedTextures.contains(id)) m_expandedTextures.remove(id);
    else m_expandedTextures.insert(id);
    Refresh();
}

QModelIndex FolderAssetModel::TextureIndex(const QString& textureId) const {
    for (int row = 0; row < rowCount(); ++row)
        if (m_entries[row].spriteId.isEmpty() && m_entries[row].textureId == textureId)
            return index(row, 0);
    return {};
}

void FolderAssetModel::QueueRefresh() {
    if (m_refreshQueued) return;
    m_refreshQueued = true;
    QTimer::singleShot(0, this, [this] {
        m_refreshQueued = false;
        Refresh();
    });
}

void FolderAssetModel::Refresh() {
    if (!m_root.isValid()) {
        if (!m_entries.empty()) {
            beginResetModel();
            m_entries.clear();
            endResetModel();
        }
        return;
    }
    // Group once per refresh, rather than scanning every sprite for each texture.
    QHash<QString, std::vector<Sprite*>> spritesByTexture;
    for (const auto& [id, sprite] : AssetManager::Get().GetAllSprites())
        if (sprite) spritesByTexture[QString::fromStdString(sprite->GetTextureID())].push_back(sprite);
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);

    std::vector<Entry> entries;
    for (int row = 0; row < m_source->rowCount(m_root); ++row) {
        const QModelIndex source = m_source->index(row, 0, m_root);
        const QFileInfo info = m_source->fsModel()->fileInfo(m_source->mapToSource(source));
        Entry entry;
        entry.source = source;
        entry.filePath = info.absoluteFilePath();
        entry.directory = info.isDir();
        entry.name = source.data().toString();
        const QString suffix = info.suffix().toLower();
        if (!entry.directory) {
            if (suffix == "texture" || suffix == "shader" || suffix == "font" || suffix == "audio")
                entry.name = QFileInfo(info.completeBaseName()).completeBaseName();
            else if (suffix == "mat" || suffix == "material" || suffix == "scene" ||
                     suffix == "tilemap" || suffix == "tileset")
                entry.name = info.completeBaseName();
            if (suffix == "texture" || suffix == "mat" || suffix == "material") {
                try {
                    const auto node = YAML::LoadFile(entry.filePath.toStdString());
                    entry.assetId = QString::fromStdString(node["id"].as<std::string>(""));
                } catch (const YAML::Exception&) {}
            }
            if (suffix == "texture") entry.textureId = entry.assetId;
        }
        auto sprites = spritesByTexture.find(entry.textureId);
        entry.hasSprites = !entry.textureId.isEmpty() && sprites != spritesByTexture.end()
                           && !sprites->empty();
        entry.expanded = entry.hasSprites && m_expandedTextures.contains(entry.textureId);
        entries.push_back(entry);
        if (!entry.expanded) continue;

        std::sort(sprites->begin(), sprites->end(), [&collator](Sprite* a, Sprite* b) {
            const int order = collator.compare(QString::fromStdString(a->GetName()),
                                                QString::fromStdString(b->GetName()));
            return order == 0 ? a->GetID() < b->GetID() : order < 0;
        });
        for (Sprite* sprite : *sprites) {
            Entry child;
            const std::string name = sprite->GetName();
            child.name = QString::fromStdString(name.empty() ? sprite->GetID() : name);
            child.spriteId = QString::fromStdString(sprite->GetID());
            child.assetId = child.spriteId;
            child.textureId = entry.textureId;
            const auto pixels = sprite->GetPixelSize();
            child.spriteSize = QSize(std::max(1, qRound(pixels.x)), std::max(1, qRound(pixels.y)));
            entries.push_back(std::move(child));
        }
    }

    // Preserve indexes/selection outside the changed span when expanding,
    // collapsing, slicing or deleting sprites. Do not reset the whole grid.
    int prefix = 0;
    const int oldCount = rowCount(), newCount = static_cast<int>(entries.size());
    while (prefix < oldCount && prefix < newCount &&
           m_entries[prefix].Key() == entries[prefix].Key()) ++prefix;
    int suffix = 0;
    while (suffix < oldCount - prefix && suffix < newCount - prefix &&
           m_entries[oldCount - suffix - 1].Key() == entries[newCount - suffix - 1].Key()) ++suffix;
    if (oldCount - suffix > prefix) {
        beginRemoveRows({}, prefix, oldCount - suffix - 1);
        m_entries.erase(m_entries.begin() + prefix, m_entries.end() - suffix);
        endRemoveRows();
    }
    if (newCount - suffix > prefix) {
        beginInsertRows({}, prefix, newCount - suffix - 1);
        m_entries.insert(m_entries.begin() + prefix, entries.begin() + prefix, entries.end() - suffix);
        endInsertRows();
    }
    m_entries = std::move(entries);
    if (rowCount()) emit dataChanged(index(0, 0), index(rowCount() - 1, 0));
}
