#include "utils/AssetPreviewDelegate.hpp"

#include <QIcon>
#include <QListView>
#include <QPainter>
#include <QPainterPath>

#include "utils/AssetThumbnails.hpp"
#include "utils/FolderAssetModel.hpp"

AssetPreviewDelegate::AssetPreviewDelegate(QListView* view, QObject* parent)
    : QStyledItemDelegate(parent), m_view(view) {}

QSize AssetPreviewDelegate::sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const {
    return {kCellWidth, kCellHeight};
}

void AssetPreviewDelegate::DrawSpriteGroup(QPainter* painter, const QRect& cell,
                                          const QModelIndex& index) const {
    const QString texture = index.data(FolderAssetModel::TextureIdRole).toString();
    auto sameRow = [&](int row) {
        const QModelIndex other = index.model()->index(row, 0);
        return other.isValid() && !other.data(FolderAssetModel::SpriteIdRole).toString().isEmpty() &&
               other.data(FolderAssetModel::TextureIdRole).toString() == texture &&
               m_view->visualRect(other).top() == cell.top();
    };
    const bool joinsLeft = sameRow(index.row() - 1);
    const bool joinsRight = sameRow(index.row() + 1);
    // Cover the thumbnail and its label with one shared sprite-group surface.
    QRectF band(cell);
    if (joinsRight) band.setRight(m_view->visualRect(index.model()->index(index.row() + 1, 0)).left());
    QPainterPath background;
    background.addRoundedRect(band, 6, 6);
    if (joinsLeft) background.addRect(QRectF(band.left(), band.top(), 6, band.height()));
    if (joinsRight) background.addRect(QRectF(band.right() - 6, band.top(), 6, band.height()));
    background.setFillRule(Qt::WindingFill);
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(63, 63, 63));
    painter->drawPath(background);
    if (!joinsLeft) {
        const qreal y = band.center().y();
        painter->drawPolygon(QPolygonF{QPointF(band.left(), y - 5),
                                      QPointF(band.left() - 5, y),
                                      QPointF(band.left(), y + 5)});
    }
}

void AssetPreviewDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                const QModelIndex& index) const {
    if (!index.isValid()) return;
    const QString sprite = index.data(FolderAssetModel::SpriteIdRole).toString();
    const QString texture = index.data(FolderAssetModel::TextureIdRole).toString();
    const QString asset = index.data(FolderAssetModel::AssetIdRole).toString();
    const bool isSprite = !sprite.isEmpty();
    const bool selected = option.state.testFlag(QStyle::State_Selected);
    const bool hovered = option.state.testFlag(QStyle::State_MouseOver);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    if (isSprite) DrawSpriteGroup(painter, option.rect, index);
    if (selected || hovered) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(selected ? QColor(65, 88, 114, 220) : QColor(95, 95, 98, 160));
        painter->drawRoundedRect(QRectF(option.rect).adjusted(1, 1, -1, -1), 6, 6);
    }

    QPixmap pixmap;
    if (isSprite) pixmap = AssetThumbnails::forSpritePreview(sprite.toStdString());
    else if (!texture.isEmpty()) pixmap = AssetThumbnails::forTexture(texture.toStdString());
    else if (!asset.isEmpty()) pixmap = AssetThumbnails::forMaterial(asset.toStdString());
    if (pixmap.isNull() && !isSprite)
        pixmap = qvariant_cast<QIcon>(index.data(Qt::DecorationRole)).pixmap(
            QSize(kThumbSize, kThumbSize), painter->device()->devicePixelRatioF());

    const QRect imageArea(option.rect.left() + (option.rect.width() - kThumbSize) / 2,
                          option.rect.top() + kThumbTopPad, kThumbSize, kThumbSize);
    if (!pixmap.isNull()) {
        // QIcon pixmaps can carry a device-pixel ratio (e.g. 2 on a 200% display).
        // Fit and center in logical coordinates, then draw into an explicit
        // target so physical image pixels never shrink or offset the preview.
        const QSizeF previewSize = pixmap.deviceIndependentSize().scaled(
            QSizeF(imageArea.size()), Qt::KeepAspectRatio);
        QRectF target(QPointF(0, 0), previewSize);
        target.moveCenter(QRectF(imageArea).center());
        painter->save();
        painter->setClipRect(imageArea, Qt::IntersectClip);
        painter->setRenderHint(QPainter::SmoothPixmapTransform, !isSprite);
        painter->drawPixmap(target, pixmap, QRectF(pixmap.rect()));
        painter->restore();
    }

    const QRect labelArea(option.rect.left() + 2, option.rect.bottom() - kLabelHeight + 1,
                          option.rect.width() - 4, kLabelHeight);
    painter->setFont(option.font);
    painter->setPen(selected ? Qt::white : option.palette.text().color());
    const QString label = painter->fontMetrics().elidedText(index.data().toString(),
                                                           Qt::ElideRight, labelArea.width());
    painter->drawText(labelArea, Qt::AlignHCenter | Qt::AlignTop, label);
    painter->restore();
}
