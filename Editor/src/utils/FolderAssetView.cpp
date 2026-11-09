#include "utils/FolderAssetView.hpp"

#include <QDrag>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>

#include "utils/AssetPreviewDelegate.hpp"
#include "utils/AssetThumbnails.hpp"
#include "utils/DragDropMime.hpp"
#include "utils/FolderAssetModel.hpp"

bool FolderAssetView::IsToggle(const QModelIndex& index, const QPoint& position) const {
    return index.data(FolderAssetModel::HasSpritesRole).toBool() &&
           AssetPreviewDelegate::ToggleRect(visualRect(index)).contains(position);
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

void FolderAssetView::mousePressEvent(QMouseEvent* event) {
    m_pressedToggle = QModelIndex();
    const QModelIndex index = indexAt(event->position().toPoint());
    if (event->button() == Qt::LeftButton && IsToggle(index, event->position().toPoint())) {
        m_pressedToggle = index;
        event->accept();
        return;
    }
    QListView::mousePressEvent(event);
}

void FolderAssetView::mouseMoveEvent(QMouseEvent* event) {
    if (m_pressedToggle.isValid()) { event->accept(); return; }
    QListView::mouseMoveEvent(event);
}

void FolderAssetView::mouseReleaseEvent(QMouseEvent* event) {
    if (m_pressedToggle.isValid()) {
        const QModelIndex index = m_pressedToggle;
        m_pressedToggle = QModelIndex();
        if (event->button() == Qt::LeftButton && IsToggle(index, event->position().toPoint()))
            Toggle(index);
        event->accept();
        return;
    }
    QListView::mouseReleaseEvent(event);
}

void FolderAssetView::mouseDoubleClickEvent(QMouseEvent* event) {
    if (IsToggle(indexAt(event->position().toPoint()), event->position().toPoint())) {
        event->accept();
        return;
    }
    QListView::mouseDoubleClickEvent(event);
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
