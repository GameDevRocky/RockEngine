#pragma once
#include <QStyledItemDelegate>
#include <QPainter>
#include "engine/core/GameObject.hpp"
#include "engine/serialization/Registry.hpp"

class SceneTreeItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit SceneTreeItemDelegate(QObject* parent = nullptr);

    // Reserve exactly the active-toggle footprint. SceneTree::item's own left
    // padding supplies the small visual gap before the GameObject icon.
    static constexpr int LEFT_MARGIN = 18;

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;
};
