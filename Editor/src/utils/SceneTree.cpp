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
#include "engine/core/UndoSystem.hpp"
#include "engine/commands/SubtreeCommand.hpp"
#include "engine/commands/ReparentCommand.hpp"
#include "engine/commands/MacroCommand.hpp"
#include <algorithm>
#include <vector>

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

    // Keep the row under the right parent when the data side reparents. This lives here
    // rather than in HierarchyGui (where it used to be, walking the scene at load time)
    // because objects created *after* the walk — new, duplicated, or restored by undo —
    // never got a subscription and their row would sit under the old parent until the
    // next full refresh. Every row is built here, so every row is covered.
    //
    // It also matters that this is the only mechanism: SceneTree::dropEvent moves the Qt
    // row itself before calling SetParent, but an undo has no such Qt-side move, so the
    // row follows the data only through this event.
    if (Transform* transform = gameObject->GetTransform()) {
        transform->Subscribe([weakTree, id](const std::any& data) {
            if (!weakTree) return false;

            const std::string& newParentTransformId = std::any_cast<const std::string&>(data);
            std::string newParentGameObjectId;
            if (!newParentTransformId.empty()) {
                Transform* parentTransform = Registry::FindInRuntime<Transform>(newParentTransformId);
                if (parentTransform && parentTransform->GetGameObject())
                    newParentGameObjectId = parentTransform->GetGameObject()->GetID();
            }

            weakTree->ReparentItem(id, newParentGameObjectId);
            return true;
        }, Transform::PARENT_CHANGED_EVENT);
    }

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
    // Multi-select. Qt's ExtendedSelection hardwires Ctrl = toggle and Shift =
    // range-extend; the viewport matches by using Ctrl for toggle too, which is why
    // its pan gesture moved off Ctrl and onto Alt.
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
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

            // Snapshot after AddGameObject so the record includes the Transform it
            // synthesised. Undo destroys the object; redo restores it with the
            // same id, so anything that referenced it still resolves.
            if (undoSystem) {
                undoSystem->Push(SubtreeCommand::RecordCreated(
                    obj, scene->SnapshotSubtree(obj), "Create GameObject"));
            }
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

    // Driven off the selection model rather than QTreeView::clicked: clicked fires
    // per-index, ignores modifiers entirely, and never fires for keyboard navigation.
    connect(selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this](const QItemSelection&, const QItemSelection&) {
        if (m_syncingSelection) return;

        std::vector<std::string> ids;
        for (const QModelIndex& index : selectionModel()->selectedRows()) {
            QString idQt = index.data(GAMEOBJECT_ID_ROLE).toString();
            if (!idQt.isEmpty()) ids.push_back(idQt.toStdString());
        }

        // selectedRows() comes back in MODEL order, not click order. Rotate the
        // current index to the back so the last-clicked row becomes the primary —
        // otherwise the inspector and gizmo pivot jump to whatever sits lowest in
        // the tree rather than what the user just clicked.
        QString currentIdQt = currentIndex().data(GAMEOBJECT_ID_ROLE).toString();
        if (!currentIdQt.isEmpty()) {
            const std::string currentId = currentIdQt.toStdString();
            auto it = std::find(ids.begin(), ids.end(), currentId);
            if (it != ids.end() && it + 1 != ids.end())
                std::rotate(it, it + 1, ids.end());
        }

        m_syncingSelection = true;
        selectionManager->SelectMany(ids);
        m_syncingSelection = false;
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

std::unique_ptr<Command> SceneTree::DuplicateOne(const std::string& id,
                                                 std::string* outCloneId) {
    GameObject* source = registry->Find<GameObject>(id);
    if (!source) return nullptr;
    Scene* scene = source->GetScene();
    if (!scene) return nullptr;

    // Capture the post-remap YAML so redo rebuilds *this* clone rather than
    // duplicating again — a second DuplicateGameObject call would mint fresh
    // UUIDs and orphan every reference captured against the first clone.
    YAML::Node snapshot;
    GameObject* clone = scene->DuplicateGameObject(source, &snapshot);
    if (!clone) return nullptr;
    if (outCloneId) *outCloneId = clone->GetID();

    // Neither pushes nor selects: the caller decides, because a multi-duplicate has
    // to land as one history entry and select every clone at the end.
    return SubtreeCommand::RecordCreated(clone, snapshot, "Duplicate " + source->GetName());
}

void SceneTree::DuplicateObject(const std::string& id) {
    std::string cloneId;
    auto command = DuplicateOne(id, &cloneId);
    if (cloneId.empty()) return;

    selectionManager->Select(cloneId);
    if (undoSystem) undoSystem->Push(std::move(command));
}

void SceneTree::DuplicateSelection() {
    // Roots only: duplicating a parent already clones its children, so also
    // duplicating a selected child would produce a stray second copy.
    std::vector<std::string> roots = selectionManager->GetSelectedRoots();
    if (roots.empty()) return;
    if (roots.size() == 1) { DuplicateObject(roots.front()); return; }

    std::vector<std::unique_ptr<Command>> commands;
    std::vector<std::string> cloneIds;
    commands.reserve(roots.size());

    for (const std::string& id : roots) {
        std::string cloneId;
        if (auto command = DuplicateOne(id, &cloneId)) {
            commands.push_back(std::move(command));
            cloneIds.push_back(cloneId);
        }
    }
    if (cloneIds.empty()) return;

    selectionManager->SelectMany(cloneIds);

    if (undoSystem) {
        auto macro = MacroCommand::Wrap(
            std::move(commands),
            "Duplicate " + std::to_string(cloneIds.size()) + " Objects");
        if (auto* m = dynamic_cast<MacroCommand*>(macro.get())) {
            m->SetSelectionAfterUndo(roots);      // undo removes the clones
            m->SetSelectionAfterRedo(cloneIds);   // redo brings them back
        }
        undoSystem->Push(std::move(macro));
    }
}

void SceneTree::DeleteSelection() {
    // Roots only, for two reasons: destroying a parent already destroys its
    // children, and a child's id would no longer resolve by the time we reached it.
    std::vector<std::string> roots = selectionManager->GetSelectedRoots();
    if (roots.empty()) return;

    std::vector<std::unique_ptr<Command>> commands;
    commands.reserve(roots.size());

    for (const std::string& id : roots) {
        GameObject* go = registry->Find<GameObject>(id);
        if (!go) continue;
        // DestroyAndRecord performs the Shutdown itself so it can snapshot first,
        // but it returns nullptr WITHOUT destroying when the object has no scene —
        // so the two branches are not interchangeable.
        if (undoSystem) {
            if (auto command = SubtreeCommand::DestroyAndRecord(go))
                commands.push_back(std::move(command));
        } else {
            go->Shutdown();
        }
    }

    if (undoSystem) {
        auto macro = MacroCommand::Wrap(
            std::move(commands),
            roots.size() == 1 ? "Delete Object"
                              : "Delete " + std::to_string(roots.size()) + " Objects");
        if (auto* m = dynamic_cast<MacroCommand*>(macro.get())) {
            // Each child SubtreeCommand re-selects the object it restores, stomping
            // the last; the macro sets the correct final selection once at the end.
            m->SetSelectionAfterUndo(roots);
            m->SetSelectionAfterRedo({});
        }
        undoSystem->Push(std::move(macro));
    }

    selectionManager->ClearSelection();
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
    // The payload carries only the primary id; OnSelectionChanged reads the whole
    // set from the manager, so it is ignored here.
    selectionSubscriptionId = selectionManager->Subscribe([this](const std::any&) {
            OnSelectionChanged();
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
    // Multi-object reparent is not implemented: GameObjectDragModel::mimeData
    // encodes a single id (and its consumer RefDropFilter is a single-valued sink),
    // so a multi-row drag would silently move only currentIndex()'s object and
    // leave the rest behind. Refuse the drop rather than half-perform it.
    if (selectionModel()->selectedRows().size() > 1) {
        event->ignore();
        return;
    }

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
        if (!scene || !childObj) return;

        auto command = ReparentCommand::Capture(childObj);
        scene->ReorderObject(childIdQt.toStdString(), newParentIdQt.toStdString(), targetRow);
        if (command && command->Commit(childObj) && undoSystem)
            undoSystem->Push(std::move(command));
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

    // Capture before Qt moves the row, so the recorded "before" is the real one.
    auto command = ReparentCommand::Capture(childObject);

    QTreeView::dropEvent(event);

    if (!event->isAccepted())
        return;

    handlingDrop = true;
    childTransform->SetParent(parentTransform, true);
    handlingDrop = false;

    // SetParent appends, so the new sibling index is read back rather than assumed.
    if (command && command->Commit(childObject) && undoSystem)
        undoSystem->Push(std::move(command));
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

void SceneTree::OnSelectionChanged() {
    if (m_syncingSelection) return;
    m_syncingSelection = true;

    QItemSelection selection;
    QModelIndex primaryIndex;
    const std::string& primaryId = selectionManager->GetPrimaryId();

    for (const std::string& id : selectionManager->GetSelectedIds()) {
        QModelIndex index = FindItemById(id);
        // Not in THIS tree — skip it, but keep going. There is one SceneTree per
        // scene and they all hear the same event.
        if (!index.isValid()) continue;

        for (QModelIndex parent = index.parent(); parent.isValid(); parent = parent.parent())
            expand(parent);

        selection.select(index, index);
        if (id == primaryId) primaryIndex = index;
    }

    // Unconditional. The old early-return-on-not-found was the cross-tree stale
    // highlight bug: a tree that contains none of the new selection must still clear
    // its own, or selecting in one scene leaves the other scene looking selected.
    if (selection.isEmpty()) {
        clearSelection();
    } else {
        selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect
                                          | QItemSelectionModel::Rows);
    }

    if (primaryIndex.isValid()) {
        // NoUpdate, not ClearAndSelect|Rows: the latter would wipe the multi-row
        // selection just applied above and leave only the primary highlighted.
        selectionModel()->setCurrentIndex(primaryIndex, QItemSelectionModel::NoUpdate);
        scrollTo(primaryIndex);
    } else {
        setCurrentIndex(QModelIndex());
    }

    m_syncingSelection = false;
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
    // Both operate on the whole selection, via the engine's SelectionManager rather
    // than currentIndex(), so they agree with what the user sees highlighted.
    if (event->key() == Qt::Key_D && (event->modifiers() & Qt::ControlModifier)) {
        DuplicateSelection();
        return;
    }
    if (event->key() == Qt::Key_Delete) {
        DeleteSelection();
        return;
    }
    QTreeView::keyPressEvent(event);
}

