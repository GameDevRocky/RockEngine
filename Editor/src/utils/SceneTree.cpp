#include "utils/SceneTree.hpp"
#include "engine/core/Scene.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/core/SelectionManager.hpp"
#include "engine/components/Transform.hpp"
#include "engine/serialization/Registry.hpp"
#include "Engine.hpp"

#include <QSizePolicy>
#include "utils/SceneTreeItemDelegate.hpp"
#include <QStandardItem>
#include <QModelIndexList>
#include <QHeaderView>
#include <QItemSelectionModel>
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

GameObjectItem* CreateGameObjectItem(QStandardItemModel* model, GameObject* gameObject) {
    if (!model || !gameObject) return nullptr;

    const std::string& id = gameObject->GetID();
    const std::string name = gameObject->GetName();
    auto* item = new GameObjectItem(name.c_str());
    item->SetGameObjectId(id);
    item->setFlags(item->flags() | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
    
    gameObject->Subscribe([model, id](){
        QString idQt = QString::fromStdString(id);
        QModelIndexList matches = model->match(
            model->index(0, 0),
            Qt::UserRole + 1,
            idQt,
            1,
            Qt::MatchExactly | Qt::MatchRecursive
        );
        if (matches.empty()) return;

        auto* obj = Registry::FindInRuntime<GameObject>(id);
        if (!obj) return;

        auto* liveItem = static_cast<GameObjectItem*>(model->itemFromIndex(matches.front()));
        if (!liveItem) return;
        liveItem->setText(obj->GetName().c_str());
    }, GameObject::NAME_CHANGED_EVENT);
    return item;
}

void AddGameObjectNode(QStandardItemModel* model, QStandardItem* parentItem, GameObject* gameObject) {
    if (!model || !parentItem || !gameObject) return;

    auto* item = CreateGameObjectItem(model, gameObject);
    if (!item) return;
    
    parentItem->appendRow(item);
    
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
    
    header()->setSectionsClickable(true);
    connect(header(), &QHeaderView::sectionClicked, this, &SceneTree::OnHeaderClicked);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    setItemDelegate(new SceneTreeItemDelegate(this));
    
    connect(this, &QTreeView::clicked, this, [this](const QModelIndex& index) {
            if (!index.isValid()) return;
            
            QString gameObjectId = index.data(Qt::UserRole + 1).toString();
            if (gameObjectId.isEmpty()) return;
            
            Container* container = Engine::Get()->GetActiveContainer();
            SelectionManager* selectionManager = container->FindSystem<SelectionManager>();
            selectionManager->Select(gameObjectId.toStdString());
            
        });
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
    scene->Subscribe([this](const std::any& data){
        const std::string& name = std::any_cast<std::string>(data);
        model->setHorizontalHeaderLabels({name.c_str()});
    }, Scene::NAME_CHANGED_EVENT);

    scene_id = scene->GetID();
    
    QStandardItem* rootItem = model->invisibleRootItem();
    rootItem->setFlags(rootItem->flags() | Qt::ItemIsDropEnabled);
    for (GameObject* rootObject : scene->GetRootObjects()) {
        AddGameObjectNode(model, rootItem, rootObject);
    }

    auto* container = Engine::Get()->GetActiveContainer();    
    auto* selectionManager = container->FindSystem<SelectionManager>();
    selectionManager->Subscribe([this](const std::any& data) {
        if (!data.has_value()) return;
        
        try {
            const std::string& selectedId = std::any_cast<const std::string&>(data);
            OnObjectSelected(selectedId);
        } catch (const std::bad_any_cast&) {
            return;
        }
    }, SelectionManager::SELECTION_CHANGED_EVENT);

    expandAll();
}

void SceneTree::dropEvent(QDropEvent* event) {
    DropIndicatorPosition pos = dropIndicatorPosition();
    if (pos == AboveItem || pos == BelowItem) {
        event->ignore();
        return;
    }

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

    handlingDrop = true;
    childTransform->SetParent(parentTransform, true);
    handlingDrop = false;
}

void SceneTree::OnObjectSelected(const std::string& id) {
    // Handle deselection
    if (id.empty()) {
        clearSelection();
        setCurrentIndex(QModelIndex());
        return;
    }

    QModelIndex index = FindItemById(id);
    if (!index.isValid()) return;

    QModelIndex parent = index.parent();
    while (parent.isValid()) {
        expand(parent);
        parent = parent.parent();
    }

    setCurrentIndex(index);
    selectionModel()->setCurrentIndex(
        index,
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows
    );

    scrollTo(index);
}

QModelIndex SceneTree::FindItemById(const std::string& id) const {
    QString idQt = QString::fromStdString(id);
    QModelIndexList matches = model->match(
        model->index(0, 0),
        GAMEOBJECT_ID_ROLE,
        idQt,
        1,
        Qt::MatchExactly | Qt::MatchRecursive
    );
    
    if (matches.empty())
        return QModelIndex();
    
    return matches.front();
}

void SceneTree::AddItem(const std::string& parentId, GameObject* child) {
    if (!child) return;

    QStandardItem* parentItem = nullptr;
    
    if (parentId.empty()) {
        parentItem = model->invisibleRootItem();
    } else {
        QModelIndex parentIndex = FindItemById(parentId);
        if (!parentIndex.isValid()) return;
        parentItem = model->itemFromIndex(parentIndex);
    }
    
    if (!parentItem) return;

    auto* item = CreateGameObjectItem(model, child);
    if (!item) return;
    
    parentItem->appendRow(item);
    
    Transform* transform = child->GetTransform();
    if (transform) {
        for (Transform* childTransform : transform->GetChildren()) {
            if (!childTransform) continue;
            GameObject* childObject = childTransform->GetGameObject();
            AddGameObjectNode(model, item, childObject);
        }
    }
}

void SceneTree::RemoveItem(const std::string& id) {
    QModelIndex index = FindItemById(id);
    if (!index.isValid()) return;
    
    QStandardItem* item = model->itemFromIndex(index);
    if (!item) return;
    
    QStandardItem* parentItem = item->parent();
    if (!parentItem) {
        parentItem = model->invisibleRootItem();
    }
    
    parentItem->removeRow(item->row());
}

void SceneTree::ReparentItem(const std::string& childId, const std::string& newParentId) {
    QModelIndex childIndex = FindItemById(childId);
    if (!childIndex.isValid()) return;
    
    QStandardItem* childItem = model->itemFromIndex(childIndex);
    if (!childItem) return;
    
    QStandardItem* oldParent = childItem->parent();
    if (!oldParent) {
        oldParent = model->invisibleRootItem();
    }
    
    QStandardItem* newParent = nullptr;
    if (newParentId.empty()) {
        newParent = model->invisibleRootItem();
    } else {
        QModelIndex newParentIndex = FindItemById(newParentId);
        if (!newParentIndex.isValid()) return;
        newParent = model->itemFromIndex(newParentIndex);
    }
    
    if (!newParent || oldParent == newParent) return;
    int row = childItem->row();
    QList<QStandardItem*> taken = oldParent->takeRow(row);
    newParent->appendRow(taken);
}

void SceneTree::OnHeaderClicked(int section) {
    Q_UNUSED(section);
    
    collapsed = !collapsed;
    
    for (int i = 0; i < model->rowCount(); ++i) {
        setRowHidden(i, QModelIndex(), collapsed);
    }

    if (collapsed) {
        int headerHeight = header()->height();
        setFixedHeight(headerHeight);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    } else {
        setMaximumHeight(16777215); 
        setMinimumHeight(0);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        updateGeometry();
        adjustSize(); 
    }
}
