#include "utils/SceneTree.hpp"
#include "engine/core/Scene.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/core/SelectionManager.hpp"
#include "engine/components/Transform.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/core/Container.hpp"
#include "Engine.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include "engine/components/BoxCollider.hpp"
#include "engine/components/RigidBody.hpp"

#include <QSizePolicy>
#include <QKeyEvent>
#include "utils/SceneTreeItemDelegate.hpp"
#include <QStandardItem>
#include <QModelIndexList>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QMenu>
#include <QEvent>
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
        if (matches.empty()) return false;

        auto* obj = Registry::FindInRuntime<GameObject>(id);
        if (!obj) return false;

        auto* liveItem = static_cast<GameObjectItem*>(model->itemFromIndex(matches.front()));
        if (!liveItem) return false;
        liveItem->setText(obj->GetName().c_str());
        return true;
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
    QIcon icon("Domain/lib/assets/icons/hamburger_icon.png");
    m_headerBtn = new QPushButton(header());
    m_headerBtn->setIcon(icon);
    m_headerBtn->setFixedSize(40, 20);
    m_headerBtn->setFlat(true);

    connect(m_headerBtn, &QPushButton::clicked, this, [this]() {
        QMenu menu(this);
        menu.addAction("New GameObject", this, [this]() {
            auto* selectionManager = Engine::Get()->GetActiveContainer()->FindSystem<SelectionManager>();
            auto* scene = Registry::FindInRuntime<Scene>(scene_id);
            if (!scene) return;
            auto* obj = new GameObject();
            obj->SetName("GameObject");
            SpriteRenderer* sr = new SpriteRenderer();
            RigidBody* rb = new RigidBody();
            BoxCollider* bc = new BoxCollider();
            Transform* t = new Transform();
            t->SetScale({10, 10});
            
            scene->AddGameObject(obj);
            obj->AddComponent(t);
            obj->AddComponent(sr);
            obj->AddComponent(rb);
            obj->AddComponent(bc);
            selectionManager->Select(obj->GetID());
        });
        menu.exec(m_headerBtn->mapToGlobal(m_headerBtn->rect().bottomLeft()));
    });
    header()->installEventFilter(this);

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
    // Unsubscribe previous subscriptions to prevent accumulation across rebuilds.
    if (!scene_id.empty()) {
        Registry* reg = Engine::Get()->GetActiveContainer()->FindSystem<Registry>();
        Scene* prevScene = reg ? reg->Find<Scene>(scene_id) : nullptr;
        if (prevScene) {
            if (sceneNameSubscriptionId != -1) prevScene->Unsubscribe(sceneNameSubscriptionId);
            if (sceneAddedSubscriptionId != -1) prevScene->Unsubscribe(sceneAddedSubscriptionId);
        }
        sceneNameSubscriptionId = -1;
        sceneAddedSubscriptionId = -1;
    }
    if (selectionSubscriptionId != -1) {
        auto* selMgr = Engine::Get()->GetActiveContainer()->FindSystem<SelectionManager>();
        if (selMgr) selMgr->Unsubscribe(selectionSubscriptionId);
        selectionSubscriptionId = -1;
    }

    model->clear();

    if (!scene) {
        model->setHorizontalHeaderLabels({"Hierarchy"});
        scene_id.clear();
        deleteLater();
        return;
    }
    header()->setFixedHeight(30);

    model->setHorizontalHeaderLabels({scene->GetName().c_str()});
    sceneNameSubscriptionId = scene->Subscribe([this](const std::any& data){
        const std::string& name = std::any_cast<std::string>(data);
        model->setHorizontalHeaderLabels({name.c_str()});
        return true;
    }, Scene::NAME_CHANGED_EVENT);

    scene_id = scene->GetID();
    
    QStandardItem* rootItem = model->invisibleRootItem();
    rootItem->setFlags(rootItem->flags() | Qt::ItemIsDropEnabled);
    for (GameObject* rootObject : scene->GetRootObjects()) {
        AddGameObjectNode(model, rootItem, rootObject);
    }

    auto* container = Engine::Get()->GetActiveContainer();    
    auto* selectionManager = container->FindSystem<SelectionManager>();
    selectionSubscriptionId = selectionManager->Subscribe([this](const std::any& data) {

            const std::string& selectedId = std::any_cast<const std::string&>(data);
            OnObjectSelected(selectedId);
            return true;
    }, SelectionManager::SELECTION_CHANGED_EVENT);

    sceneAddedSubscriptionId = scene->Subscribe([this](const std::any& data) {
        const std::string& newId = std::any_cast<const std::string&>(data);
        Registry* registry = Engine::Get()->GetActiveContainer()->FindSystem<Registry>();
        GameObject* go = registry->Find<GameObject>(newId);
        if (!go) return true;
        Transform* t = go->GetTransform();
        std::string parentId;
        if (t && t->GetParent() && t->GetParent()->GetGameObject())
            parentId = t->GetParent()->GetGameObject()->GetID();
        AddItem(parentId, go);
        return true;
    }, Scene::GAMEOBJECT_ADDED_EVENT);

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

bool SceneTree::eventFilter(QObject* obj, QEvent* event) {
    if (obj == header() && event->type() == QEvent::Resize) {
        const int margin = 4;
        m_headerBtn->move(header()->width() - m_headerBtn->width() - margin,
                          (header()->height() - m_headerBtn->height()) / 2);
    }
    return QTreeView::eventFilter(obj, event);
}

void SceneTree::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Delete) {
        QModelIndex index = currentIndex();
        if (!index.isValid()) return;
        QString idQt = index.data(GAMEOBJECT_ID_ROLE).toString();
        if (idQt.isEmpty()) return;
        Registry* registry = Engine::Get()->GetActiveContainer()->FindSystem<Registry>();
        GameObject* go = registry->Find<GameObject>(idQt.toStdString());
        if (go) go->Shutdown();
        return;
    }
    QTreeView::keyPressEvent(event);
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
