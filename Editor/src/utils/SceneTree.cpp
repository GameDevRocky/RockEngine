#include "utils/SceneTree.hpp"
#include "engine/core/Scene.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/components/Transform.hpp"
#include "engine/serialization/Registry.hpp"
#include "Engine.hpp"

#include <QStandardItem>
#include <QModelIndexList>
#include <stdexcept>

namespace {
constexpr int GAMEOBJECT_ID_ROLE = Qt::UserRole + 1;

bool WouldCreateCycle(Transform* childTransform, Transform* newParentTransform) {
    if (!childTransform || !newParentTransform)
        return false;

    Transform* current = newParentTransform;
    while (current) {
        if (current == childTransform)
            return true;
        current = current->GetParent();
    }

    return false;
}

void AddGameObjectNode(QStandardItemModel* model, QStandardItem* parentItem, GameObject* gameObject) {
    if (!model || !parentItem || !gameObject) return;

    const std::string& id = gameObject->GetID();
    const std::string name = gameObject->GetName();
    auto* item = new GamobjectItem(name.c_str());
    item->SetGameObjectId(id);
    item->setFlags(item->flags() | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
    parentItem->appendRow(item);

    QString idQt = QString::fromStdString(id);
    gameObject->Subscribe([model, idQt](){
        QModelIndexList matches = model->match(
            model->index(0, 0),
            GAMEOBJECT_ID_ROLE,
            idQt,
            1,
            Qt::MatchExactly | Qt::MatchRecursive
        );
        if (matches.empty())
            throw std::runtime_error("Item destroyed");
        
        auto* obj = Registry::FindInRuntime<GameObject>(idQt.toStdString());
        if (!obj) return;
        
        QStandardItem* item = model->itemFromIndex(matches.front());
        if (item) {
            item->setText(obj->GetName().c_str());
        }
    }, GameObject::NAME_CHANGED_EVENT);
    
    Transform* transform = gameObject->GetTransform();

    for (Transform* childTransform : transform->GetChildren()) {
        if (!childTransform) continue;
        GameObject* childObject = childTransform->GetGameObject();
        AddGameObjectNode(model, item, childObject);
    }
}
} 

SceneTree::SceneTree(QWidget* parent): QTreeView(parent) {
    model = new QStandardItemModel(this);
    model->setHorizontalHeaderLabels({"Hierarchy"});
    setModel(model);
    setHeaderHidden(false);
    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    setDefaultDropAction(Qt::MoveAction);
    setDragDropMode(QAbstractItemView::InternalMove);
    setDragDropOverwriteMode(false);
}

void SceneTree::RebuildFromScene(Scene* scene) {
    model->clear();

    if (!scene) {
        model->setHorizontalHeaderLabels({"Hierarchy"});
        scene_id.clear();
        deleteLater();
        return;
    }

    model->setHorizontalHeaderLabels({scene->GetName().c_str()});

    scene_id = scene->GetID();
    
    QStandardItem* rootItem = model->invisibleRootItem();
    rootItem->setFlags(rootItem->flags() | Qt::ItemIsDropEnabled);
    for (GameObject* rootObject : scene->GetRootObjects()) {
        AddGameObjectNode(model, rootItem, rootObject);
    }

    expandAll();
}

void SceneTree::dropEvent(QDropEvent* event) {
    const QModelIndex draggedIndex = currentIndex();
    const QString childIdQt = draggedIndex.data(GAMEOBJECT_ID_ROLE).toString();

    if (childIdQt.isEmpty() || scene_id.empty()) {
        event->ignore();
        return;
    }

    Registry* registry = Engine::Get()->GetActiveContainer()->FindSystem<Registry>();
    if (!registry) {
        event->ignore();
        return;
    }

    GameObject* childObject = registry->Find<GameObject>(childIdQt.toStdString());
    if (!childObject) {
        event->ignore();
        return;
    }

    Transform* childTransform = childObject->GetTransform();
    if (!childTransform) {
        event->ignore();
        return;
    }

    const QModelIndex dropIndex = indexAt(event->position().toPoint());
    Transform* parentTransform = nullptr;

    if (dropIndex.isValid()) {
        const QString parentIdQt = dropIndex.data(GAMEOBJECT_ID_ROLE).toString();
        GameObject* parentObject = registry->Find<GameObject>(parentIdQt.toStdString());
        if (parentObject) {
            parentTransform = parentObject->GetTransform();
        }
    }

    if (WouldCreateCycle(childTransform, parentTransform)) {
        event->ignore();
        return;
    }

    if (childTransform->GetParent() == parentTransform) {
        event->ignore();
        return;
    }

    QTreeView::dropEvent(event);

    if (!event->isAccepted())
        return;

    childTransform->SetParent(parentTransform, true);
}