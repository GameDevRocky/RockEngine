#pragma once
#include <QStyledItemDelegate>
#include <QPainter>
#include "engine/core/GameObject.hpp"
#include "engine/serialization/Registry.hpp"

class SceneTreeItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit SceneTreeItemDelegate(QObject* parent = nullptr);

    // Space reserved on the left for the active-toggle button
    static constexpr int LEFT_MARGIN = 26;

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;
};
