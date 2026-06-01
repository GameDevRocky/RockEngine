#include "utils/SceneTree.hpp"
#include "engine/core/Scene.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/core/SelectionManager.hpp"
#include "engine/core/SceneManager.hpp"
#include "engine/components/Transform.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/core/Container.hpp"
#include "Engine.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include "engine/components/BoxCollider.hpp"
#include "engine/components/RigidBody.hpp"

#include <QSizePolicy>
#include <QKeyEvent>
#include <QPointer>
#include <QMouseEvent>
#include <QScrollBar>
#include "utils/SceneTreeItemDelegate.hpp"
#include <QStandardItem>
#include <QModelIndexList>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QMenu>
#include <QEvent>
#include <stdexcept>

#include "engine/utils/EngineUtils.hpp"
using namespace EngineUtils;
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
    item->setIcon(QIcon(EngineUtils::GetAssetPath("Domain/lib/assets/icons/cube.png").c_str()));
    if (!gameObject->GetActive())
        item->setFlags(item->flags() & ~Qt::ItemIsEnabled);

    QPointer<QStandardItemModel> weakModel = model;
    gameObject->Subscribe([weakModel, id](){
        if (!weakModel) return false;
        QString idQt = QString::fromStdString(id);
        QModelIndexList matches = weakModel->match(
            weakModel->index(0, 0),
            Qt::UserRole + 1,
            idQt,
            1,
            Qt::MatchExactly | Qt::MatchRecursive
        );
        if (matches.empty()) return false;

        auto* obj = Registry::FindInRuntime<GameObject>(id);
        if (!obj) return false;

        auto* liveItem = static_cast<GameObjectItem*>(weakModel->itemFromIndex(matches.front()));
        if (!liveItem) return false;
        liveItem->setText(obj->GetName().c_str());
        return true;
    }, GameObject::NAME_CHANGED_EVENT);

    gameObject->Subscribe([weakModel, id](){
        if (!weakModel) return false;
        QString idQt = QString::fromStdString(id);
        QModelIndexList matches = weakModel->match(
            weakModel->index(0, 0),
            Qt::UserRole + 1,
            idQt,
            1,
            Qt::MatchExactly | Qt::MatchRecursive
        );
        if (matches.empty()) return false;

        auto* obj = Registry::FindInRuntime<GameObject>(id);
        if (!obj) return false;

        auto* liveItem = weakModel->itemFromIndex(matches.front());
        if (!liveItem) return false;
        if (obj->GetActive())
            liveItem->setFlags(liveItem->flags() | Qt::ItemIsEnabled);
        else
            liveItem->setFlags(liveItem->flags() & ~Qt::ItemIsEnabled);
        return true;
    }, GameObject::ACTIVE_CHANGED_EVENT);

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

void UnsubscribeFromContainer(Container* container, const std::string& scene_id,
                               int selId, int nameId, int addedId) {
    if (!container) return;

    if (selId != -1) {
        if (auto* selMgr = container->FindSystem<SelectionManager>())
            selMgr->Unsubscribe(selId);
    }

    if (!scene_id.empty() && (nameId != -1 || addedId != -1)) {
        if (auto* reg = container->FindSystem<Registry>()) {
            if (auto* scene = reg->Find<Scene>(scene_id)) {
                if (nameId != -1) scene->Unsubscribe(nameId);
                if (addedId != -1) scene->Unsubscribe(addedId);
            }
        }
    }
}

} // namespace

SceneTree::SceneTree(QWidget* parent): QTreeView(parent) {
    model = new QStandardItemModel(this);
    auto* headerItem = new QStandardItem(style()->standardIcon(QStyle::SP_DirIcon), "Hierarchy");
    model->setHorizontalHeaderItem(0, headerItem);
    setModel(model);
    setHeaderHidden(false);
    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    setDefaultDropAction(Qt::MoveAction);
    setDragDropMode(QAbstractItemView::InternalMove);
    setDragDropOverwriteMode(false);

    header()->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(header(), &QHeaderView::customContextMenuRequested, this, [this](const QPoint& pos) {
        QMenu menu(this);

        menu.addAction("Remove Scene", this, [this](){
            sceneManager->RemoveScene(scene_id);
        });

        menu.addAction("New GameObject", this, [this]() {
            auto* scene = registry->Find<Scene>(scene_id);
            if (!scene) return;
            auto* obj = new GameObject();
            obj->SetName("GameObject");
            scene->AddGameObject(obj);
            selectionManager->Select(obj->GetID());
        });

        QAction* collapseAction = menu.addAction("Collapse");
        collapseAction->setCheckable(true);
        collapseAction->setChecked(collapsed);
        connect(collapseAction, &QAction::triggered, this, [this]() {
            collapsed = !collapsed;
            for (int i = 0; i < model->rowCount(); ++i)
                setRowHidden(i, QModelIndex(), collapsed);
            if (collapsed) {
                setFixedHeight(header()->height());
                setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            } else {
                setMaximumHeight(16777215);
                setMinimumHeight(0);
                setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
                updateGeometry();
                adjustSize();
            }
        });
        menu.exec(header()->mapToGlobal(pos));
    });

    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setItemDelegate(new SceneTreeItemDelegate(this));

    // Floating active-toggle button (lives in the viewport, shown on hover)
    m_activeBtn = new QToolButton(viewport());
    m_activeBtn->setObjectName("ActiveToggleBtn");
    m_activeBtn->setCheckable(true);
    m_activeBtn->setFixedSize(18, 18);
    m_activeBtn->hide();

    viewport()->setMouseTracking(true);
    viewport()->installEventFilter(this);

    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this]() {
        m_activeBtn->hide();
        m_hoveredGoId.clear();
    });

    connect(m_activeBtn, &QToolButton::clicked, this, [this](bool checked) {
        if (m_hoveredGoId.empty()) return;
        auto* go = Registry::FindInRuntime<GameObject>(m_hoveredGoId);
        if (go) go->SetActive(checked);
    });

    connect(this, &QTreeView::clicked, this, [this](const QModelIndex& index) {
            if (!index.isValid()) return;
            QString gameObjectId = index.data(Qt::UserRole + 1).toString();
            if (gameObjectId.isEmpty()) return;
            selectionManager->Select(gameObjectId.toStdString());
            
        });
}

SceneTree::~SceneTree() {
    UnsubscribeFromContainer(Engine::Get()->GetEditorContainer(), scene_id,
        selectionSubscriptionId, sceneNameSubscriptionId, sceneAddedSubscriptionId);
    if (Engine::Get()->GetRuntimeContainer()) {
        UnsubscribeFromContainer(Engine::Get()->GetRuntimeContainer(), scene_id,
            selectionSubscriptionId, sceneNameSubscriptionId, sceneAddedSubscriptionId);
    }
}

void SceneTree::RebuildFromScene(Scene* scene) {
    UnsubscribeFromContainer(Engine::Get()->GetEditorContainer(), scene_id,
        selectionSubscriptionId, sceneNameSubscriptionId, sceneAddedSubscriptionId);
    if (Engine::Get()->GetRuntimeContainer()) {
        UnsubscribeFromContainer(Engine::Get()->GetRuntimeContainer(), scene_id,
            selectionSubscriptionId, sceneNameSubscriptionId, sceneAddedSubscriptionId);
    }
    sceneNameSubscriptionId = -1;
    sceneAddedSubscriptionId = -1;
    selectionSubscriptionId = -1;

    model->clear();
    m_activeBtn->hide();
    m_hoveredGoId.clear();

    auto setHeader = [this](const QString& name) {
        model->setHorizontalHeaderItem(0, new QStandardItem(
            QIcon(EngineUtils::GetAssetPath("Domain/lib/assets/icons/scene_icon.png").c_str()), name));
    };

    if (!scene) {
        setHeader("Hierarchy");
        scene_id.clear();
        deleteLater();
        return;
    }
    header()->setFixedHeight(30);

    setHeader(scene->GetName().c_str());
    sceneNameSubscriptionId = scene->Subscribe([this](const std::any& data){
        const std::string& name = std::any_cast<std::string>(data);
        model->setHorizontalHeaderItem(0, new QStandardItem(
            style()->standardIcon(QStyle::SP_DirIcon), name.c_str()));
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
    if (obj == viewport()) {
        if (event->type() == QEvent::MouseMove) {
            auto* me = static_cast<QMouseEvent*>(event);
            QModelIndex index = indexAt(me->pos());
            if (index.isValid())
                OnItemEntered(index);
            else {
                m_activeBtn->hide();
                m_hoveredGoId.clear();
            }
        } else if (event->type() == QEvent::Leave) {
            m_activeBtn->hide();
            m_hoveredGoId.clear();
        }
    }
    return QTreeView::eventFilter(obj, event);
}

void SceneTree::OnItemEntered(const QModelIndex& index) {
    QVariant idVar = index.data(Qt::UserRole + 1);
    if (!idVar.isValid()) {
        m_activeBtn->hide();
        m_hoveredGoId.clear();
        return;
    }

    m_hoveredGoId = idVar.toString().toStdString();
    auto* go = Registry::FindInRuntime<GameObject>(m_hoveredGoId);

    QRect itemRect = visualRect(index);
    int btnSize = m_activeBtn->height();
    m_activeBtn->move(itemRect.left() + 4,
                      itemRect.top() + (itemRect.height() - btnSize) / 2);

    m_activeBtn->blockSignals(true);
    m_activeBtn->setChecked(go ? go->GetActive() : false);
    m_activeBtn->blockSignals(false);

    m_activeBtn->show();
    m_activeBtn->raise();
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
