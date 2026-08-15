#include "dock-widgets/InspectorGui.hpp"

#include "Engine.hpp"
#include "engine/core/SelectionManager.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/components/ScriptComponent.hpp"
#include "utils/CollapsableWidget.hpp"
#include "component-widgets/ComponentHeader.hpp"
#include "utils/InspectorVisitor.hpp"
#include "engine/components/Transform.hpp"
#include "engine/rendering/core/Sprite.hpp"
#include "engine/rendering/core/Material.hpp"
#include "engine/rendering/core/Texture2D.hpp"
#include "engine/rendering/core/Shader.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/serialization/SerializableFactory.hpp"
#include "engine/components/Component.hpp"
#include "utils/ComponentPickerWidget.hpp"
#include "utils/RefDropFilter.hpp"
#include "engine/core/UndoSystem.hpp"
#include "engine/core/Container.hpp"
#include "engine/commands/ComponentCommand.hpp"
#include "engine/commands/ComponentOrderCommand.hpp"
#include "utils/DragDropMime.hpp"
#include <algorithm>
#include <functional>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QApplication>
#include <QPushButton>
#include <QSizePolicy>
#include <QScrollBar>

namespace {
    void clearLayout(QLayout* layout)
    {
        if (!layout) return;

        QLayoutItem* item;
        while ((item = layout->takeAt(0)) != nullptr)
        {
            if (QWidget* widget = item->widget())
            {
                // Immediate deletion is safer for rapid Inspector switching
                delete widget; 
            }
            else if (QLayout* childLayout = item->layout())
            {
                clearLayout(childLayout);
               
                delete childLayout;
            }

            delete item;
        }
    }

    // Accepts the same component MIME drag that AI chat consumes, but interprets
    // a drop back onto the current Inspector as an ordered component move. This
    // keeps one header gesture useful both inside and outside the Inspector.
    class ComponentReorderDropController final : public QObject {
    public:
        using ReorderCallback =
            std::function<void(const std::string& movedId,
                               const std::string& nextVisibleId)>;

        ComponentReorderDropController(QWidget* eventTarget, QWidget* rowsHost,
                                       ReorderCallback onReorder, QObject* parent)
            : QObject(parent),
              m_eventTarget(eventTarget),
              m_host(rowsHost),
              m_onReorder(std::move(onReorder)) {
            if (m_eventTarget) {
                m_eventTarget->setAcceptDrops(true);
                // Observe at application level so component drops still win over
                // nested asset-reference fields, which are drop targets of their
                // own. Events outside this Inspector viewport pass through.
                if (QApplication* app = qobject_cast<QApplication*>(
                        QApplication::instance()))
                    app->installEventFilter(this);
            }
        }

        void RegisterRow(QWidget* row, std::string componentId) {
            m_rows.push_back(row);
            m_componentIds.push_back(std::move(componentId));
        }

    protected:
        bool eventFilter(QObject* watched, QEvent* event) override {
            if (!m_eventTarget || !m_host)
                return QObject::eventFilter(watched, event);

            switch (event->type()) {
            case QEvent::DragEnter: {
                auto* drag = static_cast<QDragEnterEvent*>(event);
                QPoint hostPosition;
                if (!insideInspector(watched, drag->position().toPoint(), hostPosition) ||
                    indexOfMime(drag->mimeData()) < 0) break;
                accept(drag);
                return true;
            }
            case QEvent::DragMove: {
                auto* drag = static_cast<QDragMoveEvent*>(event);
                QPoint hostPosition;
                if (!insideInspector(watched, drag->position().toPoint(), hostPosition) ||
                    indexOfMime(drag->mimeData()) < 0) break;
                showIndicatorAt(targetSlot(hostPosition));
                accept(drag);
                return true;
            }
            case QEvent::DragLeave:
                hideIndicator();
                return false;
            case QEvent::Drop: {
                auto* drop = static_cast<QDropEvent*>(event);
                QPoint hostPosition;
                if (!insideInspector(watched, drop->position().toPoint(), hostPosition))
                    break;
                const int from = indexOfMime(drop->mimeData());
                if (from < 0) break;

                const int slot = targetSlot(hostPosition);
                const int count = static_cast<int>(m_componentIds.size());
                const int to = std::clamp(slot > from ? slot - 1 : slot,
                                          0, count - 1);
                hideIndicator();
                accept(drop);

                if (to != from && m_onReorder) {
                    std::vector<std::string> visibleOrder = m_componentIds;
                    const std::string movedId =
                        visibleOrder[static_cast<std::size_t>(from)];
                    visibleOrder.erase(visibleOrder.begin() + from);
                    visibleOrder.insert(visibleOrder.begin() + to, movedId);
                    const std::string nextVisibleId = to + 1 < count
                        ? visibleOrder[static_cast<std::size_t>(to + 1)]
                        : std::string{};
                    m_onReorder(movedId, nextVisibleId);
                }
                return true;
            }
            default:
                break;
            }
            return QObject::eventFilter(watched, event);
        }

    private:
        static void accept(QDropEvent* event) {
            event->setDropAction(Qt::CopyAction);
            event->accept();
        }

        int indexOfMime(const QMimeData* mime) const {
            if (!mime || !mime->hasFormat(kComponentMimeType)) return -1;
            const std::string id = QString::fromUtf8(
                mime->data(kComponentMimeType)).trimmed().toStdString();
            const auto it = std::find(m_componentIds.begin(), m_componentIds.end(), id);
            return it == m_componentIds.end()
                ? -1
                : static_cast<int>(std::distance(m_componentIds.begin(), it));
        }

        bool insideInspector(QObject* watched, const QPoint& eventPosition,
                             QPoint& hostPosition) const {
            auto* eventWidget = qobject_cast<QWidget*>(watched);
            if (!eventWidget || !m_eventTarget || !m_host) return false;
            const QPoint globalPosition = eventWidget->mapToGlobal(eventPosition);
            const QPoint viewportPosition = m_eventTarget->mapFromGlobal(globalPosition);
            if (!m_eventTarget->rect().contains(viewportPosition)) return false;
            hostPosition = m_host->mapFromGlobal(globalPosition);
            return true;
        }

        int targetSlot(const QPoint& hostPosition) const {
            if (!m_host || m_rows.empty()) return 0;
            const int y = hostPosition.y();
            for (std::size_t i = 0; i < m_rows.size(); ++i) {
                const QWidget* row = m_rows[i];
                if (row && y < row->y() + row->height() / 2)
                    return static_cast<int>(i);
            }
            return static_cast<int>(m_rows.size());
        }

        void showIndicatorAt(int slot) {
            if (!m_host) return;
            if (!m_indicator) {
                m_indicator = new QWidget(m_host);
                m_indicator->setFixedHeight(2);
                m_indicator->setStyleSheet("background: rgb(87, 126, 100);");
                m_indicator->setAttribute(Qt::WA_TransparentForMouseEvents);
            }

            int y = 0;
            if (m_rows.empty()) {
                y = 0;
            } else if (slot >= static_cast<int>(m_rows.size())) {
                y = m_rows.back()->geometry().bottom() + 1;
            } else if (m_rows[static_cast<std::size_t>(slot)]) {
                y = m_rows[static_cast<std::size_t>(slot)]->y();
            }
            m_indicator->setGeometry(0, y - 1, m_host->width(), 2);
            m_indicator->show();
            m_indicator->raise();
        }

        void hideIndicator() {
            if (m_indicator) m_indicator->hide();
        }

        QWidget* m_eventTarget = nullptr;
        QWidget* m_host = nullptr;
        ReorderCallback m_onReorder;
        std::vector<QWidget*> m_rows;
        std::vector<std::string> m_componentIds;
        QWidget* m_indicator = nullptr;
    };
}


InspectorGui::InspectorGui(QWidget* parent) : QWidget(parent)
{

    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(0);
    
    scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mainLayout->addWidget(scrollArea);

    // Inspector-wide script drop target: dragging a .py script from the Folder
    // view onto a GameObject's inspector adds a ScriptComponent running it. The
    // filter lives on the scroll viewport (persists across inspector rebuilds);
    // a drop onto the existing script field is handled by that field's own
    // RefDropFilter instead. Gated so only GameObject selections accept the drop.
    QWidget* viewport = scrollArea->viewport();
    viewport->setAcceptDrops(true);
    viewport->installEventFilter(new RefDropFilter(
        Properties::PropDesc().Tag(Properties::Tags::SCRIPT),
        [this](const std::string& ref) { AddScriptToSelectedObject(ref); },
        viewport,
        []() {
            auto* c = Engine::Get()->GetActiveContainer();
            auto* sm = c ? c->FindSystem<SelectionManager>() : nullptr;
            return sm && dynamic_cast<GameObject*>(sm->GetSerializable()) != nullptr;
        }));
}

void InspectorGui::AddScriptToSelectedObject(const std::string& scriptRef)
{
    // scriptRef is "module:class" (see RefDropFilter's SCRIPT resolution).
    const auto colon = scriptRef.find(':');
    if (colon == std::string::npos) return;
    const std::string module    = scriptRef.substr(0, colon);
    const std::string className = scriptRef.substr(colon + 1);
    if (module.empty() || className.empty()) return;

    auto* container = Engine::Get()->GetActiveContainer();
    if (!container) return;
    auto* selMgr = container->FindSystem<SelectionManager>();
    auto* obj = selMgr ? dynamic_cast<GameObject*>(selMgr->GetSerializable()) : nullptr;
    if (!obj) return;   // GameObjects only

    auto* created = SerializableFactory::Create("ScriptComponent");
    auto* comp = dynamic_cast<ScriptComponent*>(created);
    if (!comp) { delete created; return; }

    // AddComponent attaches + registers the component and fires ADD_COMPONENT_EVENT
    // (which rebuilds this inspector). Assign the script and record undo AFTER, so
    // the snapshot RecordAdded takes carries the chosen module/class.
    obj->AddComponent(comp);
    auto* registry = container->FindSystem<Registry>();
    if (!registry || !registry->Find<Component>(comp->GetID())) return;   // add rejected

    comp->SetScript(module, className);

    if (auto* undoSystem = container->FindSystem<UndoSystem>())
        undoSystem->Push(ComponentCommand::RecordAdded(obj, comp));
}

void InspectorGui::Init(){ 
    auto* engine = Engine::Get();
    // Entering play mode: the runtime container's SelectionManager is a fresh copy
    // (Copy(Container*) doesn't carry subscribers), so wire up a subscription to it.
    engine->Subscribe([this](){
        SubscribeToSelector();
        return true;
    }, Engine::ENTER_PLAY_MODE_EVENT);
    // Exiting play mode: the editor SelectionManager still holds the subscription made
    // in Init (the editor container outlives play mode), so do NOT re-subscribe — that
    // would stack a duplicate SELECTION_CHANGED handler on the editor manager every
    // play cycle, making each editor selection trigger N inspector rebuilds. Just
    // refresh the inspector for the editor's current selection.
    engine->Subscribe([this](){
        // ExitPlayMode() has already deleted the runtime container, so any inspector
        // subscriptions from a play-mode selection point at freed runtime objects.
        // Their ~Observable already dropped the subscriptions; the handles just
        // dangle. Drop them WITHOUT unsubscribing — calling Unsubscribe on the freed
        // objects is a use-after-free. (Editor-object subs are unsubscribed normally
        // on enter, where the editor objects are still alive.)
        m_inspectorSubs.clear();
        OnObjectSelected(selectionManager->GetSelectedId());
        return true;
    }, Engine::EXIT_PLAY_MODE_EVENT);
    SubscribeToSelector();

    // Poll the inspected script(s) for Python-side field changes (see
    // PollScriptFields). 20 Hz is responsive for an inspector and only touches the
    // currently-selected object's fields, and only while in play mode.
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(50);
    connect(m_pollTimer, &QTimer::timeout, this, [this]() {
        // Value refresh runs in every mode (physics in play, gizmo drags/undo in
        // editor); the script-field poll is runtime-only and self-gates.
        RefreshDirtyValues();
        PollScriptFields();
    });
    m_pollTimer->start();

    std::cout << "InspectorGui Initialized" << std::endl;
}

void InspectorGui::RefreshDirtyValues(){
    for (auto& refresh : m_valueRefreshers)
        refresh();
}

void InspectorGui::RequestRebuild(){
    if (m_currentInspectedId.empty()) return;
    // Queued, not immediate: callers reach this from inside a property widget's
    // own signal handler, and OnObjectSelected destroys those widgets. Same
    // rationale as the script-reload rebuild below.
    QMetaObject::invokeMethod(this, [this]() {
        OnObjectSelected(m_currentInspectedId);
    }, Qt::QueuedConnection);
}

void InspectorGui::PollScriptFields(){
    if (m_polledScriptIds.empty()) return;
    // Python only mutates fields while its update loop runs — i.e. in Runtime mode.
    // At edit time nothing changes silently, so skip the GIL work entirely.
    Container* active = Engine::Get()->GetActiveContainer();
    if (!active || active->GetMode() != Container::Mode::Runtime) return;
    for (const auto& id : m_polledScriptIds)
        if (auto* sc = Registry::FindInRuntime<ScriptComponent>(id))
            sc->PollFieldChanges();
}

void InspectorGui::SubscribeToSelector(){
    selectionManager->Subscribe([this](std::any data){
        const std::string& id = std::any_cast<std::string>(data);
        OnObjectSelected(id);
        return true;
    }, SelectionManager::SELECTION_CHANGED_EVENT);
    OnObjectSelected(selectionManager->GetSelectedId());
}

void InspectorGui::OnObjectSelected(const std::string& id)
{
    // Preserve the scroll position across a rebuild of the SAME object (add/remove
    // component, script hot-reload) so the view doesn't snap back to the top. A
    // rebuild for a DIFFERENT selection intentionally resets to the top.
    const bool sameObject = (!id.empty() && id == m_currentInspectedId);
    const int savedScroll = sameObject ? scrollArea->verticalScrollBar()->value() : 0;
    m_currentInspectedId = id;

    // Rebuilding the inspector ends any in-progress coalescing gesture: the
    // widgets that were accumulating into the current undo entry are about to be
    // destroyed, so the next edit must start a fresh entry.
    if (Container* active = Engine::Get()->GetActiveContainer()) {
        if (auto* undoSystem = active->FindSystem<UndoSystem>())
            undoSystem->BreakMergeChain();
    }

    for (auto& [compId, subId] : m_scriptReloadSubs) {
        auto* sc = Registry::FindInRuntime<ScriptComponent>(compId);
        if (sc) sc->Unsubscribe(subId);
    }
    m_scriptReloadSubs.clear();
    m_polledScriptIds.clear();

    for (auto& [matId, subId] : m_materialShaderSubs)
        if (auto* mat = AssetManager::Get().GetMaterial(matId))
            mat->Unsubscribe(subId);
    m_materialShaderSubs.clear();

    // Tear down the per-rebuild property/header subscriptions from the previous
    // selection. Without this they pile up on long-lived objects/assets, growing
    // the cost of every notify and of SelectionManager's Unsubscribe scan.
    for (auto& [obs, subId] : m_inspectorSubs)
        obs->Unsubscribe(subId);
    m_inspectorSubs.clear();
    // Deferred value refreshers belong to the widgets we're about to destroy.
    m_valueRefreshers.clear();

    if (contentWidget) {
        // Detach the old content from the scroll area before scheduling its deletion.
        // scrollArea->setWidget() below would otherwise delete the previous widget
        // *synchronously*, which is a use-after-free when this rebuild was triggered
        // from inside that widget's own event handler (e.g. clicking a read-only
        // sprite row in the texture inspector). takeWidget() hands ownership back to
        // us so deleteLater() can defer destruction until the event finishes.
        scrollArea->takeWidget();
        contentWidget->deleteLater();
        contentWidget = nullptr;
    }

    if (id.empty()) return;

    // Resolve selection — could be a GameObject or an asset (Sprite, Material, etc.)
    auto* selMgr = Engine::Get()->GetActiveContainer()->FindSystem<SelectionManager>();
    Serializable* selectable = selMgr ? selMgr->GetSerializable() : nullptr;
    if (!selectable) return;

    auto* obj = dynamic_cast<GameObject*>(selectable);

    contentWidget = new QWidget();
    QVBoxLayout* contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setAlignment(Qt::AlignTop);

    if (obj) {
        // ── GameObject path: header + per-component sections ─────────────────
        InspectorVisitor visitor;
        ObjectHeader* objectHeader = new ObjectHeader();
        obj->Accept(&visitor);
        objectHeader->Bind(obj->GetID());
        for (auto& s : visitor.GetSubscriptions())       m_inspectorSubs.push_back(s);
        for (auto& s : objectHeader->GetSubscriptions())  m_inspectorSubs.push_back(s);
        for (auto& r : visitor.GetRefreshers())           m_valueRefreshers.push_back(r);
        auto* content = visitor.GetContent();
        if (content) objectHeader->AddWidget(content);
        contentLayout->addWidget(objectHeader);

        // Rebuild whenever a component is added, removed, or reordered on this
        // object. Torn down with the rest of m_inspectorSubs on the next rebuild.
        {
            std::string capturedId = id;
            int addSub = obj->Subscribe([this, capturedId](std::any) {
                OnObjectSelected(capturedId);
                return true;
            }, GameObject::ADD_COMPONENT_EVENT);
            m_inspectorSubs.emplace_back(obj, addSub);

            int removeSub = obj->Subscribe([this, capturedId](std::any) {
                OnObjectSelected(capturedId);
                return true;
            }, GameObject::REMOVE_COMPONENT_EVENT);
            m_inspectorSubs.emplace_back(obj, removeSub);

            int orderSub = obj->Subscribe([this](std::any) {
                // The source ComponentHeader may still be inside QDrag::exec();
                // defer rebuilding so it survives until that drag unwinds.
                RequestRebuild();
                return true;
            }, GameObject::COMPONENT_ORDER_CHANGED_EVENT);
            m_inspectorSubs.emplace_back(obj, orderSub);
        }

        // The header starts a normal component MIME drag. This controller catches
        // that drag only when it returns to this Inspector, adds insertion-line
        // feedback, and commits the authoritative engine order. The same drag can
        // still leave the Inspector and be copied into AI chat as context.
        auto* componentReorder = new ComponentReorderDropController(
            scrollArea->viewport(),
            contentWidget,
            [objectId = obj->GetID()](const std::string& movedId,
                                      const std::string& nextVisibleId) {
                Container* active = Engine::Get()->GetActiveContainer();
                Registry* registry = active ? active->FindSystem<Registry>() : nullptr;
                GameObject* owner = registry ? registry->Find<GameObject>(objectId) : nullptr;
                if (!owner) return;

                // Remove the dragged component from the full order, then insert it
                // immediately before its next visible neighbor (or at the end).
                // Components without Inspector content retain their relative order.
                std::vector<std::string> reducedOrder = owner->GetComponentOrder();
                const auto movedIt = std::find(reducedOrder.begin(), reducedOrder.end(), movedId);
                if (movedIt == reducedOrder.end()) return;
                reducedOrder.erase(movedIt);

                std::size_t targetIndex = reducedOrder.size();
                if (!nextVisibleId.empty()) {
                    const auto nextIt = std::find(
                        reducedOrder.begin(), reducedOrder.end(), nextVisibleId);
                    if (nextIt == reducedOrder.end()) return;
                    targetIndex = static_cast<std::size_t>(
                        std::distance(reducedOrder.begin(), nextIt));
                }

                auto command = ComponentOrderCommand::Capture(owner, movedId);
                if (!command || !owner->MoveComponent(movedId, targetIndex)) return;

                UndoSystem* undoSystem = active ? active->FindSystem<UndoSystem>() : nullptr;
                if (undoSystem && command->Commit(owner))
                    undoSystem->Push(std::move(command));
            },
            contentWidget);

        for (auto* comp : obj->GetAllComponents()) {
            InspectorVisitor compVisitor;
            comp->Accept(&compVisitor);
            if (!compVisitor.HasContent()) continue;
            content = compVisitor.GetContent();
            ComponentHeader* compWidget = new ComponentHeader(comp->GetTypeName());
            compWidget->Bind(comp->GetID());
            for (auto& s : compVisitor.GetSubscriptions()) m_inspectorSubs.push_back(s);
            for (auto& s : compWidget->GetSubscriptions())  m_inspectorSubs.push_back(s);
            for (auto& r : compVisitor.GetRefreshers())     m_valueRefreshers.push_back(r);
            compWidget->AddWidget(content);
            contentLayout->addWidget(compWidget);
            componentReorder->RegisterRow(compWidget, comp->GetID());

            if (auto* sc = dynamic_cast<ScriptComponent*>(comp)) {
                std::string capturedId = id;
                int subId = sc->Subscribe([this, capturedId](std::any) {
                    // Queue the rebuild: SCRIPT_RELOADED_EVENT can fire from within a
                    // widget callback (the script-selector's SetScript), so defer to
                    // avoid deleting inspector widgets mid-callback.
                    QMetaObject::invokeMethod(this, [this, capturedId]() {
                        OnObjectSelected(capturedId);
                    }, Qt::QueuedConnection);
                    return true;
                }, ScriptComponent::SCRIPT_RELOADED_EVENT);
                m_scriptReloadSubs.emplace_back(sc->GetID(), subId);
                m_polledScriptIds.push_back(sc->GetID());
            }
        }

        QPushButton* addComponentButton = new QPushButton("Add Component");
        addComponentButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        contentLayout->addWidget(addComponentButton);

        // Popup a list of every registered component type the object doesn't already
        // have; picking one creates it via the factory, attaches it, and rebuilds the
        // inspector so the new section appears.
        connect(addComponentButton, &QPushButton::clicked, this, [this, obj, addComponentButton]() {
            std::vector<std::string> items;
            for (const auto& name : SerializableFactory::GetRegisteredTypeNames())
                // Only hide a type if it's single-instance and already present; every
                // other type can be added multiple times (e.g. several scripts).
                if (!(Component::IsSingleton(name) && obj->HasComponentByName(name)))
                    items.push_back(name);

            auto* picker = new ComponentPickerWidget(std::move(items), this);
            picker->setFixedWidth(addComponentButton->width());
            picker->onSelected = [this, obj](const std::string& typeName) {
                // Guard only single-instance components against duplication; multiples
                // of any other type are allowed.
                if (Component::IsSingleton(typeName) && obj->HasComponentByName(typeName)) return;
                auto* comp = dynamic_cast<Component*>(SerializableFactory::Create(typeName));
                if (!comp) return;
                obj->AddComponent(comp);  // fires ADD_COMPONENT_EVENT → inspector rebuilds

                // Record after the add so the snapshot carries the component's
                // initialised state, and only if AddComponent actually kept it —
                // its singleton guard deletes rejected components.
                Container* active = Engine::Get()->GetActiveContainer();
                auto* undoSystem = active ? active->FindSystem<UndoSystem>() : nullptr;
                auto* registry = active ? active->FindSystem<Registry>() : nullptr;
                if (undoSystem && registry && registry->Find<Component>(comp->GetID()))
                    undoSystem->Push(ComponentCommand::RecordAdded(obj, comp));
            };
            picker->move(addComponentButton->mapToGlobal(QPoint(0, addComponentButton->height())));
            picker->show();
        });

        QWidget* spacer = new QWidget();
        spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        spacer->setFixedHeight(256);
        contentLayout->addWidget(spacer);
        
    } else {
        // ── Asset path: single section, no component loop ────────────────────
        InspectorVisitor visitor;
        selectable->Accept(&visitor);
        for (auto& s : visitor.GetSubscriptions()) m_inspectorSubs.push_back(s);
        for (auto& r : visitor.GetRefreshers())    m_valueRefreshers.push_back(r);
        ComponentHeader* header = new ComponentHeader(selectable->GetTypeName());
        // Do NOT call header->Bind() — it looks up a Component in the Registry and
        // self-destructs if not found. Assets live in AssetManager, not the Registry.
        if (auto* content = visitor.GetContent())
            header->AddWidget(content);
        contentLayout->addWidget(header);

        if (auto* mat = dynamic_cast<Material*>(selectable)) {
            int subId = mat->Subscribe([this, id](std::any) {
                QMetaObject::invokeMethod(this, [this, id]() {
                    OnObjectSelected(id);
                }, Qt::QueuedConnection);
                return true;
            }, Material::SHADER_CHANGED_EVENT);
            m_materialShaderSubs.emplace_back(id, subId);
        }
    }

    scrollArea->setWidget(contentWidget);

    // Restore the scroll position after the new content lays out. The scrollbar's
    // range is 0 until the layout is computed, so setting it now would clamp to 0;
    // defer to the next event-loop turn when the range is valid.
    if (sameObject && savedScroll > 0) {
        QTimer::singleShot(0, this, [this, savedScroll]() {
            if (scrollArea) scrollArea->verticalScrollBar()->setValue(savedScroll);
        });
    }
}
