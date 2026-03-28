#include "utils/SceneTreeItemDelegate.hpp"
#include <QAbstractItemModel>
#include <QWidget>

SceneTreeItemDelegate::SceneTreeItemDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

QSize SceneTreeItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    QSize sz = QStyledItemDelegate::sizeHint(option, index);
    sz.setHeight(24); // Custom height, adjust as needed
    return sz;
}

void SceneTreeItemDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const {
    // Call base implementation to set the new text
    QStyledItemDelegate::setModelData(editor, model, index);

    // Retrieve the new name from the model
    QString newName = model->data(index, Qt::EditRole).toString();

    // Retrieve the GameObject ID from the item
    QVariant idVar = model->data(index, Qt::UserRole + 1);
    if (!idVar.isValid()) return;
    std::string gameObjectId = idVar.toString().toStdString();

    // Find the GameObject and update its name
    GameObject* obj = Registry::FindInRuntime<GameObject>(gameObjectId);
    if (obj) {
        std::string nameStr = newName.toStdString();
        obj->SetName(nameStr);
    }
}
