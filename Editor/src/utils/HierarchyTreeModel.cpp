#include "utils/HierarchyTreeModel.hpp"
#include "engine/core/Scene.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/components/Transform.hpp"
#include "engine/serialization/Registry.hpp"
#include "Engine.hpp"
#include <algorithm>

// HierarchyTreeItem implementation
HierarchyTreeItem::HierarchyTreeItem(const std::string& objId, HierarchyTreeItem* parent)
    : parent(parent), gameObjectId(objId) {
}

HierarchyTreeItem::~HierarchyTreeItem() {
    children.clear();
}

HierarchyTreeItem* HierarchyTreeItem::child(int row) const {
    if (row < 0 || row >= static_cast<int>(children.size()))
        return nullptr;
    return children[row].get();
}

int HierarchyTreeItem::childCount() const {
    return static_cast<int>(children.size());
}

int HierarchyTreeItem::row() const {
    if (!parent)
        return 0;

    auto it = std::find_if(parent->children.begin(), parent->children.end(),
        [this](const std::unique_ptr<HierarchyTreeItem>& child) {
            return child.get() == this;
        });

    if (it != parent->children.end())
        return std::distance(parent->children.begin(), it);

    return 0;
}

void HierarchyTreeItem::appendChild(std::unique_ptr<HierarchyTreeItem> child) {
    children.push_back(std::move(child));
}

void HierarchyTreeItem::clear() {
    children.clear();
}

// HierarchyTreeModel implementation
HierarchyTreeModel::HierarchyTreeModel(Scene* scene, QObject* parent)
    : QAbstractItemModel(parent), scene(scene),
      rootItem(std::make_unique<HierarchyTreeItem>("")) {
    if (scene) {
        BuildTreeFromScene();
    }
}

QModelIndex HierarchyTreeModel::index(int row, int column, const QModelIndex& parent) const {
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    HierarchyTreeItem* parentItem;
    if (!parent.isValid())
        parentItem = rootItem.get();
    else
        parentItem = static_cast<HierarchyTreeItem*>(parent.internalPointer());

    HierarchyTreeItem* childItem = parentItem->child(row);
    if (childItem)
        return createIndex(row, column, childItem);

    return QModelIndex();
}

QModelIndex HierarchyTreeModel::parent(const QModelIndex& index) const {
    if (!index.isValid())
        return QModelIndex();

    HierarchyTreeItem* childItem = static_cast<HierarchyTreeItem*>(index.internalPointer());
    HierarchyTreeItem* parentItem = childItem->parent;

    if (parentItem == rootItem.get() || !parentItem)
        return QModelIndex();

    return createIndex(parentItem->row(), 0, parentItem);
}

int HierarchyTreeModel::rowCount(const QModelIndex& parent) const {
    HierarchyTreeItem* parentItem;
    if (parent.column() > 0)
        return 0;

    if (!parent.isValid())
        parentItem = rootItem.get();
    else
        parentItem = static_cast<HierarchyTreeItem*>(parent.internalPointer());

    return parentItem->childCount();
}

int HierarchyTreeModel::columnCount(const QModelIndex& parent) const {
    Q_UNUSED(parent);
    return 1; // Only one column: GameObject name
}

QVariant HierarchyTreeModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid())
        return QVariant();

    if (role != Qt::DisplayRole && role != Qt::EditRole)
        return QVariant();

    HierarchyTreeItem* item = static_cast<HierarchyTreeItem*>(index.internalPointer());
    if (!item || item->gameObjectId.empty())
        return QVariant();

    Engine* engine = Engine::Get();
    Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();

    Serializable* serializable = registry->Find(item->gameObjectId);
    GameObject* gameObj = dynamic_cast<GameObject*>(serializable);

    if (gameObj)
        return QString::fromStdString(gameObj->GetName());

    return QString::fromStdString(item->gameObjectId);
}

Qt::ItemFlags HierarchyTreeModel::flags(const QModelIndex& index) const {
    if (!index.isValid())
        return Qt::NoItemFlags;

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void HierarchyTreeModel::SetScene(Scene* newScene) {
    if (scene == newScene)
        return;

    beginResetModel();
    scene = newScene;
    rootItem->clear();
    if (scene) {
        BuildTreeFromScene();
    }
    endResetModel();
}

void HierarchyTreeModel::Rebuild() {
    beginResetModel();
    rootItem->clear();
    if (scene) {
        BuildTreeFromScene();
    }
    endResetModel();
}

void HierarchyTreeModel::BuildTreeFromScene() {
    if (!scene)
        return;

    // Get all root objects (objects with no parent transform)
    std::vector<GameObject*> rootObjects = scene->GetRootObjects();
    if (rootObjects.empty()){
        std::cout << "Empty Root Objects" << std::endl;
    }

    for (GameObject* obj : rootObjects) {

        if (!obj)
            continue;
        std::cout << obj->GetName() << std::endl;
        auto treeItem = std::make_unique<HierarchyTreeItem>(obj->GetID(), rootItem.get());
        AddGameObjectAndChildren(treeItem.get(), obj);
        rootItem->appendChild(std::move(treeItem));
    }
}

void HierarchyTreeModel::AddGameObjectAndChildren(HierarchyTreeItem* parentItem, GameObject* gameObj) {
    if (!gameObj || !parentItem)
        return;

    // Get Transform component to access children
    Transform* transform = gameObj->GetTransform();
    if (!transform)
        return;

    // Add all children of this transform
    std::vector<Transform*> children = transform->GetChildren();
    for (Transform* childTransform : children) {
        if (!childTransform)
            continue;

        // Each Transform belongs to a GameObject; we need to find which one
        // Transforms are stored in the Registry, so we need to find the GameObject
        // that has this Transform as its transform component
        
        std::vector<GameObject*> allObjects = scene->GetAllGameObjects();
        for (GameObject* candidate : allObjects) {
            if (!candidate)
                continue;

            Transform* candTransform = candidate->GetTransform();
            if (candTransform && candTransform->GetID() == childTransform->GetID()) {
                // Found the GameObject that owns this child Transform
                auto childItem = std::make_unique<HierarchyTreeItem>(
                    candidate->GetID(), parentItem);
                AddGameObjectAndChildren(childItem.get(), candidate);
                parentItem->appendChild(std::move(childItem));
                break;
            }
        }
    }
}
