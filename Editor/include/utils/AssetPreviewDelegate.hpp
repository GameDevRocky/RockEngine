#pragma once

#include <QStyledItemDelegate>

class QListView;

class AssetPreviewDelegate final : public QStyledItemDelegate {
public:
    static constexpr int kThumbSize = 48;
    static constexpr int kThumbTopPad = 8;
    static constexpr int kLabelGap = 8;
    static constexpr int kLabelHeight = 24;
    static constexpr int kCellWidth = 80;
    static constexpr int kCellHeight = kThumbTopPad + kThumbSize + kLabelGap + kLabelHeight;

    explicit AssetPreviewDelegate(QListView* view, QObject* parent = nullptr);
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    bool helpEvent(QHelpEvent* event, QAbstractItemView* view,
                   const QStyleOptionViewItem& option, const QModelIndex& index) override;
    static QRect ToggleRect(const QRect& cell);

private:
    void DrawSpriteGroup(QPainter* painter, const QRect& cell, const QModelIndex& index) const;
    QListView* m_view;
};
