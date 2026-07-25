#pragma once
#include <QWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSizePolicy>
#include <QIcon>
#include <QLabel>
#include <QPixmap>
#include <QDropEvent>
#include <QTreeView>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QMenu>
#include <QToolButton>
#include <memory>
#include "engine/core/Scene.hpp"
#include "engine/core/SelectionManager.hpp"
#include "engine/core/Command.hpp"
#include "engine/core/UndoSystem.hpp"
#include "Engine.hpp"
#include "engine/serialization/Registry.hpp"

using namespace EngineUtils;

class GameObject;

class SceneTree : public QTreeView{
    Q_OBJECT

    public:
        SceneTree(QWidget* parent = nullptr);
        ~SceneTree();
        void RebuildFromScene(Scene* scene);
        void AddItem(const std::string& parentId, GameObject* child);
        void RemoveItem(const std::string& id);
        void ReparentItem(const std::string& childId, const std::string& newParentId);

        QStandardItemModel* GetModel() const { return model; }
        const std::string& GetSceneId() const { return scene_id; }
        bool IsHandlingDrop() const { return handlingDrop; }

    protected:
        void dropEvent(QDropEvent* event) override;
        bool eventFilter(QObject* obj, QEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;
        // Selection is committed to the engine on click-release, not on press, so
        // press-and-drag (reparent/reorder) no longer selects the dragged object.
        void mousePressEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;
        void startDrag(Qt::DropActions supportedActions) override;

    private:
        QModelIndex FindItemById(const std::string& id) const;
        // Clone the object as a sibling and select the clone. Used by the per-item
        // context menu, which acts on the clicked row rather than the selection.
        void DuplicateObject(const std::string& id);
        // Clones one object and returns the command WITHOUT pushing or selecting,
        // so a multi-duplicate can group them into a single history entry.
        std::unique_ptr<Command> DuplicateOne(const std::string& id, std::string* outCloneId);
        // Whole-selection operations, both grouped into one undo entry. Roots only —
        // an object whose ancestor is also selected is already covered by it.
        void DuplicateSelection();
        void DeleteSelection();
        // Reparents every selected root under newParentId (empty = scene root) as one
        // undo entry. insertRow < 0 appends (drop ONTO a row); insertRow >= 0 places
        // the roots contiguously from that sibling index (drop BETWEEN rows). Roots
        // only, for the same reason as the two above.
        void ReparentSelection(const std::string& newParentId, int insertRow);
        // Pushes the engine's current selection into this tree's highlight. Reads
        // the manager directly rather than taking the event payload, since the
        // payload only carries the primary id.
        void OnSelectionChanged();
        // Push the tree's current highlight into the engine's SelectionManager.
        // Called on a click-release (and keyboard nav), never mid-press/drag.
        void PushSelectionToEngine();
        void OnItemEntered(const QModelIndex& index);
        // Resize the view to fit its visible rows (scrollbars are off; the outer
        // scroll area handles overflow). Call after any content/expansion change.
        void UpdateHeight();
        // Move a single row to match the data-side sibling order (driven by
        // Scene::ORDER_CHANGED_EVENT). Any reparent has already happened via ReparentItem.
        void ReorderItem(const std::string& id);
        // Re-highlight a row after a data-driven move (ReparentItem / ReorderItem):
        // takeRow drops the QItemSelectionModel entry, and only a live drag reapplies
        // it via Qt's own internal move. Undo/redo and programmatic reparents move rows
        // the same way but have no such drag, so without this the object stays selected
        // in the engine while the tree shows nothing highlighted.
        void ReselectIfSelected(const std::string& id);

        std::string scene_id;
        QStandardItemModel* model = nullptr;
        bool handlingDrop = false;
        // Set between mouse press and release; while true the tree->engine
        // selection sync is deferred so a press that turns into a drag doesn't
        // select. m_draggingItems records that the press became an item drag.
        bool m_mouseDown = false;
        bool m_draggingItems = false;
        // Guards the selection round trip in BOTH directions: tree -> engine (so the
        // engine's notify doesn't bounce back and rewrite the tree mid-signal) and
        // engine -> tree (so Qt's selectionChanged doesn't bounce back into the engine).
        bool m_syncingSelection = false;
        bool collapsed = false;
        int selectionSubscriptionId = -1;
        int sceneNameSubscriptionId = -1;
        int sceneAddedSubscriptionId = -1;
        int sceneOrderSubscriptionId = -1;

        QToolButton* m_activeBtn = nullptr;
        std::string m_hoveredGoId;

        Proxy<SceneManager> sceneManager;
        Proxy<Registry> registry;
        Proxy<SelectionManager> selectionManager;
        // Resolves through the active container, so hierarchy edits made during
        // play mode are recorded on the runtime world's history and thrown away
        // with it, never on the editor's.
        Proxy<UndoSystem> undoSystem;
};

class GameObjectItem : public QStandardItem {
    public:
        static constexpr int Type = QStandardItem::UserType + 1;

        GameObjectItem() = default;
        explicit GameObjectItem(const QString& text) : QStandardItem(text) {}
        int type() const override { return Type; }

        void SetGameObjectId(const std::string& id) {
            gameobject_id = id;
            setData(QString::fromStdString(id), Qt::UserRole + 1);
        }

        int GetSubId() {return subId;}

        void SetSubId(int id){
            this->subId = id;
        }

        const std::string& GetGameObjectId() const { return gameobject_id; }

    private:
        std::string gameobject_id;
        int subId = -1;

};