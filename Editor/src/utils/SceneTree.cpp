#include "utils/SceneTree.hpp"
#include "engine/core/Scene.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/components/Transform.hpp"

#include <QStandardItem>

namespace {
void AddGameObjectNode(QStandardItem* parentItem, GameObject* gameObject) {
    if (!parentItem || !gameObject) return;

    const std::string& id = gameObject->GetID();
    const std::string name = gameObject->GetName().empty() ? id : gameObject->GetName();
    auto* item = new QStandardItem(QString::fromStdString(name));
    item->setData(QString::fromStdString(id), Qt::UserRole + 1);
    parentItem->appendRow(item);

    Transform* transform = gameObject->GetTransform();
    if (!transform) return;

    for (Transform* childTransform : transform->GetChildren()) {
        if (!childTransform) continue;
        GameObject* childObject = childTransform->GetGameObject();
        AddGameObjectNode(item, childObject);
    }
}
} // namespace

SceneTree::SceneTree(QWidget* parent): QTreeView(parent) {
    model = new QStandardItemModel(this);
    model->setHorizontalHeaderLabels({"Hierarchy"});
    setModel(model);
    setHeaderHidden(false);
}

void SceneTree::RebuildFromScene(Scene* scene) {
    model->clear();
    model->setHorizontalHeaderLabels({scene->GetName().c_str()});

    if (!scene) {
        scene_id.clear();
        return;
    }

    scene_id = scene->GetID();
    QStandardItem* rootItem = model->invisibleRootItem();
    for (GameObject* rootObject : scene->GetRootObjects()) {
        AddGameObjectNode(rootItem, rootObject);
    }

    expandAll();
}