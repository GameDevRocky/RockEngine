#include "utils/FolderAssetView.hpp"

#include <QDrag>
#include <QKeyEvent>
#include <QMimeData>
#include <QPushButton>
#include <QResizeEvent>
#include <QSet>
#include <QTimer>

#include "utils/AssetThumbnails.hpp"
#include "utils/DragDropMime.hpp"
#include "utils/FolderAssetModel.hpp"

void FolderAssetView::SetAssetModel(FolderAssetModel* assets) {
    ClearToggleButtons();
    setModel(assets);
    auto queueSync = [this] { QueueToggleSync(); };
    connect(assets, &QAbstractItemModel::rowsInserted, this, queueSync);
    connect(assets, &QAbstractItemModel::rowsRemoved, this, queueSync);
    connect(assets, &QAbstractItemModel::dataChanged, this, queueSync);
    connect(assets, &QAbstractItemModel::layoutChanged, this, queueSync);
    connect(assets, &QAbstractItemModel::modelReset, this, queueSync);
    connect(assets, &QAbstractItemModel::modelAboutToBeReset, this,
            [this] { ClearToggleButtons(); });
    QueueToggleSync();
}

QRect FolderAssetView::ToggleRect(const QRect& cell) {
    constexpr int size = 24;
    // Center the button on the cell's right boundary, so half sits outside it.
    return QRect(cell.right() + 1 - size / 2, cell.top() + (cell.height() - size) / 2,
                 size, size);
}

void FolderAssetView::QueueToggleSync() {
    if (m_toggleSyncQueued) return;
    m_toggleSyncQueued = true;
    QTimer::singleShot(0, this, [this] {
        m_toggleSyncQueued = false;
        SyncToggleButtons();
    });
}

void FolderAssetView::ClearToggleButtons() {
    for (QPushButton* button : m_toggleButtons) {
        button->hide();
        button->deleteLater();
    }
    m_toggleButtons.clear();
}

void FolderAssetView::SyncToggleButtons() {
    if (!model()) return;
    QSet<QString> visibleTextures;
    const QRect visible = viewport()->rect();
    for (int row = 0; row < model()->rowCount(); ++row) {
        const QModelIndex index = model()->index(row, 0);
        if (!index.data(FolderAssetModel::HasSpritesRole).toBool()) continue;
        const QRect buttonRect = ToggleRect(visualRect(index));
        if (!buttonRect.intersects(visible)) continue;

        const QString id = index.data(FolderAssetModel::TextureIdRole).toString();
        visibleTextures.insert(id);
        QPushButton* button = m_toggleButtons.value(id, nullptr);
        if (!button) {
            button = new QPushButton(viewport());
            button->setObjectName("spriteDisclosureButton");
            button->setFocusPolicy(Qt::StrongFocus);
            button->setStyleSheet(
                "QPushButton#spriteDisclosureButton { background: #3d3d3d; border: 1px solid #737373; "
                "border-radius: 12px; color: #d2d2d2; font-size: 12px; padding: 0; }"
                "QPushButton#spriteDisclosureButton:hover { background: #555; }"
                "QPushButton#spriteDisclosureButton:focus { border-color: #ddd; }");
            connect(button, &QPushButton::clicked, this, [this, id] {
                auto* assets = static_cast<FolderAssetModel*>(model());
                const QModelIndex texture = assets->TextureIndex(id);
                if (texture.isValid()) Toggle(texture);
            });
            m_toggleButtons.insert(id, button);
        }
        const bool expanded = index.data(FolderAssetModel::ExpandedRole).toBool();
        const QString action = expanded ? "Hide sprites" : "Show sprites";
        button->setText(expanded ? QStringLiteral("◀") : QStringLiteral("▶"));
        button->setToolTip(action);
        button->setAccessibleName(action + " for " + index.data(Qt::DisplayRole).toString());
        if (button->geometry() != buttonRect) button->setGeometry(buttonRect);
        button->show();
    }
    for (auto it = m_toggleButtons.begin(); it != m_toggleButtons.end();) {
        if (visibleTextures.contains(it.key())) { ++it; continue; }
        it.value()->hide();
        it.value()->deleteLater();
        it = m_toggleButtons.erase(it);
    }
}

void FolderAssetView::Toggle(const QModelIndex& index) {
    auto* assets = static_cast<FolderAssetModel*>(model());
    const QString textureId = index.data(FolderAssetModel::TextureIdRole).toString();
    if (index.data(FolderAssetModel::ExpandedRole).toBool() &&
        !currentIndex().data(FolderAssetModel::SpriteIdRole).toString().isEmpty() &&
        currentIndex().data(FolderAssetModel::TextureIdRole).toString() == textureId)
        setCurrentIndex(index);
    assets->ToggleExpanded(index);
}

void FolderAssetView::resizeEvent(QResizeEvent* event) {
    QListView::resizeEvent(event);
    QueueToggleSync();
}

void FolderAssetView::scrollContentsBy(int dx, int dy) {
    QListView::scrollContentsBy(dx, dy);
    SyncToggleButtons();
}

void FolderAssetView::keyPressEvent(QKeyEvent* event) {
    const QModelIndex current = currentIndex();
    if (event->key() == Qt::Key_Left &&
        !current.data(FolderAssetModel::SpriteIdRole).toString().isEmpty()) {
        const QModelIndex texture = static_cast<FolderAssetModel*>(model())->TextureIndex(
            current.data(FolderAssetModel::TextureIdRole).toString());
        if (texture.isValid()) { Toggle(texture); event->accept(); return; }
    }
    if (current.data(FolderAssetModel::HasSpritesRole).toBool() &&
        ((event->key() == Qt::Key_Right && !current.data(FolderAssetModel::ExpandedRole).toBool()) ||
         (event->key() == Qt::Key_Left && current.data(FolderAssetModel::ExpandedRole).toBool()))) {
        Toggle(current);
        event->accept();
        return;
    }
    QListView::keyPressEvent(event);
}

void FolderAssetView::startDrag(Qt::DropActions supportedActions) {
    QMimeData* mime = model()->mimeData(selectionModel()->selectedIndexes());
    if (!mime->hasFormat(kSpriteMimeType)) {
        delete mime;
        QListView::startDrag(supportedActions & Qt::CopyAction);
        return;
    }
    QDrag drag(this);
    drag.setMimeData(mime);
    QPixmap pixmap = AssetThumbnails::forSpritePreview(
        QString::fromUtf8(mime->data(kSpriteMimeType)).toStdString());
    if (!pixmap.isNull()) {
        if (pixmap.width() > 96 || pixmap.height() > 96)
            pixmap = pixmap.scaled(96, 96, Qt::KeepAspectRatio, Qt::FastTransformation);
        drag.setPixmap(pixmap);
        drag.setHotSpot(QPoint(pixmap.width() / 2, pixmap.height() / 2));
    }
    drag.exec(Qt::CopyAction);
}
