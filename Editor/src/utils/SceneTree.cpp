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
#include <functional>
#include <unordered_map>
#include "engine/core/RuntimeObject.hpp"

#include "engine/utils/EngineUtils.hpp"
#include "utils/DragDropMime.hpp"
#include <QMimeData>
using namespace EngineUtils;
namespace {
constexpr int GAMEOBJECT_ID_ROLE = Qt::UserRole + 1;

// Standard item model that also stamps the dragged GameObject's id onto the
// drag's mime data (in addition to the built-in internal-move payload), so
// inspector reference widgets can accept a Hierarchy item dropped onto them.
// Internal reordering is untouched -- the original mime data is preserved.
class GameObjectDragModel : public QStandardItemModel {
public:
    using QStandardItemModel::QStandardItemModel;

    QMimeData* mimeData(const QModelIndexList& indexes) const override {
        QMimeData* data = QStandardItemModel::mimeData(indexes);
        if (!data) return data;
        for (const QModelIndex& idx : indexes) {
            const QString goId = idx.data(GAMEOBJECT_ID_ROLE).toString();
            if (!goId.isEmpty()) {
                data->setData(kGameObjectMimeType, goId.toUtf8());
                break;   // dragging is single-selection; first valid id wins
            }
        }
        return data;
    }
};

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

GameObjectItem* CreateGameObjectItem(SceneTree* tree, GameObject* gameObject) {
    if (!tree || !gameObject) return nullptr;
    QStandardItemModel* model = tree->GetModel();
    if (!model) return nullptr;

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

    QPointer<SceneTree> weakTree = tree;
    gameObject->Subscribe([weakTree, id]() {
        if (!weakTree) return false;
        weakTree->RemoveItem(id);
        return false;
    }, RuntimeObject::SHUTDOWN_EVENT);

    return item;
}

void AddGameObjectNode(SceneTree* tree, QStandardItem* parentItem, GameObject* gameObject) {
    if (!tree || !parentItem || !gameObject) return;

    auto* item = CreateGameObjectItem(tree, gameObject);
    if (!item) return;

    parentItem->appendRow(item);

    Transform* transform = gameObject->GetTransform();

    for (Transform* childTransform : transform->GetChildren()) {
        if (!childTransform) continue;
        GameObject* childObject = childTransform->GetGameObject();
        AddGameObjectNode(tree, item, childObject);
    }
}

void UnsubscribeFromContainer(Container* container, const std::string& scene_id,
                               int selId, int nameId, int addedId, int orderId) {
    if (!container) return;

    if (selId != -1) {
        if (auto* selMgr = container->FindSystem<SelectionManager>())
            selMgr->Unsubscribe(selId);
    }

    if (!scene_id.empty() && (nameId != -1 || addedId != -1 || orderId != -1)) {
        if (auto* reg = container->FindSystem<Registry>()) {
            if (auto* scene = reg->Find<Scene>(scene_id)) {
                if (nameId != -1) scene->Unsubscribe(nameId);
                if (addedId != -1) scene->Unsubscribe(addedId);
                if (orderId != -1) scene->Unsubscribe(orderId);
            }
        }
    }
}

} // namespace

SceneTree::SceneTree(QWidget* parent): QTreeView(parent) {
    model = new GameObjectDragModel(this);
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

        menu.addAction("Save Scene", this, [this](){
            if (sceneManager) sceneManager->SaveScene(scene_id);
        });

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
            UpdateHeight();
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

    // Per-item context menu. Separate from the header menu above, which is
    // connected to header()'s own customContextMenuRequested.
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QTreeView::customContextMenuRequested, this, [this](const QPoint& pos) {
        QModelIndex index = indexAt(pos);
        if (!index.isValid()) return;
        QString idQt = index.data(GAMEOBJECT_ID_ROLE).toString();
        if (idQt.isEmpty()) return;

        QMenu menu(this);
        menu.addAction("Duplicate", this, [this, id = idQt.toStdString()]() {
            DuplicateObject(id);
        });
        menu.exec(viewport()->mapToGlobal(pos));
    });

    connect(this, &QTreeView::expanded, this, [this](const QModelIndex&) { UpdateHeight(); });
    connect(this, &QTreeView::collapsed, this, [this](const QModelIndex&) { UpdateHeight(); });
}

void SceneTree::DuplicateObject(const std::string& id) {
    GameObject* source = registry->Find<GameObject>(id);
    if (!source) return;
    Scene* scene = source->GetScene();
    if (!scene) return;
    if (GameObject* clone = scene->DuplicateGameObject(source))
        selectionManager->Select(clone->GetID());
}

SceneTree::~SceneTree() {
    UnsubscribeFromContainer(Engine::Get()->GetEditorContainer(), scene_id,
        selectionSubscriptionId, sceneNameSubscriptionId, sceneAddedSubscriptionId, sceneOrderSubscriptionId);
    if (Engine::Get()->GetRuntimeContainer()) {
        UnsubscribeFromContainer(Engine::Get()->GetRuntimeContainer(), scene_id,
            selectionSubscriptionId, sceneNameSubscriptionId, sceneAddedSubscriptionId, sceneOrderSubscriptionId);
    }
}

void SceneTree::RebuildFromScene(Scene* scene) {
    UnsubscribeFromContainer(Engine::Get()->GetEditorContainer(), scene_id,
        selectionSubscriptionId, sceneNameSubscriptionId, sceneAddedSubscriptionId, sceneOrderSubscriptionId);
    if (Engine::Get()->GetRuntimeContainer()) {
        UnsubscribeFromContainer(Engine::Get()->GetRuntimeContainer(), scene_id,
            selectionSubscriptionId, sceneNameSubscriptionId, sceneAddedSubscriptionId, sceneOrderSubscriptionId);
    }
    sceneNameSubscriptionId = -1;
    sceneAddedSubscriptionId = -1;
    selectionSubscriptionId = -1;
    sceneOrderSubscriptionId = -1;

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
        AddGameObjectNode(this, rootItem, rootObject);
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

    sceneOrderSubscriptionId = scene->Subscribe([this](const std::any& data) {
        const std::string& movedId = std::any_cast<const std::string&>(data);
        ReorderItem(movedId);
        return true;
    }, Scene::ORDER_CHANGED_EVENT);

    expandAll();
    UpdateHeight();
}

void SceneTree::dropEvent(QDropEvent* event) {
    DropIndicatorPosition pos = dropIndicatorPosition();

    // Sibling reorder (drop between items). We ignore Qt's internal move entirely and
    // drive the model from the data side via Scene::ORDER_CHANGED_EVENT.
    if (pos == AboveItem || pos == BelowItem) {
        event->ignore();

        const QModelIndex draggedIndex = currentIndex();
        const QString childIdQt = draggedIndex.data(GAMEOBJECT_ID_ROLE).toString();
        const QModelIndex dropIndex = indexAt(event->position().toPoint());
        if (childIdQt.isEmpty() || scene_id.empty() || !dropIndex.isValid()) return;

        const QString targetIdQt = dropIndex.data(GAMEOBJECT_ID_ROLE).toString();
        if (targetIdQt == childIdQt) return; // dropped onto itself

        // New parent = the drop target's parent; target row = its sibling position.
        const QModelIndex targetParentIndex = dropIndex.parent();
        const QString newParentIdQt = targetParentIndex.isValid()
            ? targetParentIndex.data(GAMEOBJECT_ID_ROLE).toString() : QString();
        int targetRow = dropIndex.row();
        if (pos == BelowItem) targetRow += 1;

        Registry* registry = Engine::Get()->GetActiveContainer()->FindSystem<Registry>();
        if (!registry) return;

        GameObject* childObj = registry->Find<GameObject>(childIdQt.toStdString());
        GameObject* newParentObj = newParentIdQt.isEmpty()
            ? nullptr : registry->Find<GameObject>(newParentIdQt.toStdString());
        if (childObj && newParentObj &&
            WouldCreateCycle(childObj->GetTransform(), newParentObj->GetTransform()))
            return;

        Scene* scene = registry->Find<Scene>(scene_id);
        if (!scene) return;
        scene->ReorderObject(childIdQt.toStdString(), newParentIdQt.toStdString(), targetRow);
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

void SceneTree::UpdateHeight() {
    if (collapsed) {
        setFixedHeight(header()->height());
        updateGeometry();
        return;
    }

    int rows = 0;
    std::function<void(const QModelIndex&)> countVisible = [&](const QModelIndex& parent) {
        const int n = model->rowCount(parent);
        for (int i = 0; i < n; ++i) {
            if (isRowHidden(i, parent)) continue;
            const QModelIndex idx = model->index(i, 0, parent);
            ++rows;
            if (isExpanded(idx)) countVisible(idx);
        }
    };
    countVisible(QModelIndex());

    constexpr int ROW_HEIGHT = 24; // matches SceneTreeItemDelegate::sizeHint
    const int total = header()->height() + rows * ROW_HEIGHT + 2 * frameWidth() + 2;
    setFixedHeight(total);
    updateGeometry();
}

void SceneTree::ReorderItem(const std::string& id) {
    Registry* registry = Engine::Get()->GetActiveContainer()->FindSystem<Registry>();
    if (!registry) return;
    GameObject* obj = registry->Find<GameObject>(id);
    if (!obj) return;
    Transform* t = obj->GetTransform();
    if (!t) return;

    // Resolve the object's parent and its desired index among siblings from the data.
    std::string parentId;
    if (Transform* p = t->GetParent(); p && p->GetGameObject())
        parentId = p->GetGameObject()->GetID();

    QStandardItem* parentItem = nullptr;
    int targetIndex = -1;
    if (parentId.empty()) {
        parentItem = model->invisibleRootItem();
        Scene* scene = registry->Find<Scene>(scene_id);
        if (!scene) return;
        auto roots = scene->GetRootObjects();
        for (int i = 0; i < (int)roots.size(); ++i)
            if (roots[i] && roots[i]->GetID() == id) { targetIndex = i; break; }
    } else {
        QModelIndex pIdx = FindItemById(parentId);
        if (!pIdx.isValid()) return;
        parentItem = model->itemFromIndex(pIdx);
        GameObject* pObj = registry->Find<GameObject>(parentId);
        if (!pObj || !pObj->GetTransform()) return;
        auto children = pObj->GetTransform()->GetChildren();
        for (int i = 0; i < (int)children.size(); ++i)
            if (children[i] && children[i]->GetGameObject() &&
                children[i]->GetGameObject()->GetID() == id) { targetIndex = i; break; }
    }
    if (!parentItem || targetIndex < 0) return;

    QModelIndex idx = FindItemById(id);
    if (!idx.isValid()) return;
    QStandardItem* item = model->itemFromIndex(idx);
    if (!item) return;
    QStandardItem* curParent = item->parent();
    if (!curParent) curParent = model->invisibleRootItem();

    if (curParent == parentItem && item->row() == targetIndex) return; // already placed

    QList<QStandardItem*> taken = curParent->takeRow(item->row());
    if (targetIndex > parentItem->rowCount()) targetIndex = parentItem->rowCount();
    parentItem->insertRow(targetIndex, taken);
    UpdateHeight();
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

    auto* item = CreateGameObjectItem(this, child);
    if (!item) return;

    parentItem->appendRow(item);

    Transform* transform = child->GetTransform();
    if (transform) {
        for (Transform* childTransform : transform->GetChildren()) {
            if (!childTransform) continue;
            GameObject* childObject = childTransform->GetGameObject();
            AddGameObjectNode(this, item, childObject);
        }
    }

    UpdateHeight();
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
    UpdateHeight();
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
    UpdateHeight();
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
    if (event->key() == Qt::Key_D && (event->modifiers() & Qt::ControlModifier)) {
        QModelIndex index = currentIndex();
        if (!index.isValid()) return;
        QString idQt = index.data(GAMEOBJECT_ID_ROLE).toString();
        if (idQt.isEmpty()) return;
        DuplicateObject(idQt.toStdString());
        return;
    }
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
