#include "utils/SceneTreeItemDelegate.hpp"
#include "utils/SceneTree.hpp"
#include <QAbstractItemModel>
#include <QWidget>

SceneTreeItemDelegate::SceneTreeItemDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

QSize SceneTreeItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    QSize sz = QStyledItemDelegate::sizeHint(option, index);
    sz.setHeight(24);
    return sz;
}

void SceneTreeItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    opt.rect.setLeft(option.rect.left() + LEFT_MARGIN);

    // Inactive GameObjects paint with Qt's disabled visual state (dimmed icon and
    // text) but the model flags stay Qt::ItemIsEnabled, so the row stays selectable
    // — only paint-time state changes, not the item's actual enabled-ness.
    if (!index.data(GameObjectItem::ActiveRole).toBool())
        opt.state &= ~QStyle::State_Enabled;

    QStyledItemDelegate::paint(painter, opt, index);
}

void SceneTreeItemDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const {
    QStyledItemDelegate::setModelData(editor, model, index);
    QString newName = model->data(index, Qt::EditRole).toString();
    QVariant idVar = model->data(index, Qt::UserRole + 1);
    if (!idVar.isValid()) return;
    std::string gameObjectId = idVar.toString().toStdString();
    GameObject* obj = Registry::FindInRuntime<GameObject>(gameObjectId);
    if (obj) {
        const std::string& nameStr = newName.toStdString();
        obj->SetName(nameStr);
    }
}
