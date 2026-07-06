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
#include "engine/serialization/SerializableFactory.hpp"
#include "engine/components/Component.hpp"
#include "utils/ComponentPickerWidget.hpp"
#include <QPushButton>
#include <QSizePolicy>

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
}


InspectorGui::InspectorGui(QWidget* parent) : QWidget(parent)
{
    setMinimumWidth(200);

    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(0);
    
    scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mainLayout->addWidget(scrollArea);
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
    
    std::cout << "InspectorGui Initialized" << std::endl;
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
    for (auto& [compId, subId] : m_scriptReloadSubs) {
        auto* sc = Registry::FindInRuntime<ScriptComponent>(compId);
        if (sc) sc->Unsubscribe(subId);
    }
    m_scriptReloadSubs.clear();

    for (auto& [mat, subId] : m_materialShaderSubs)
        mat->Unsubscribe(subId);
    m_materialShaderSubs.clear();

    // Tear down the per-rebuild property/header subscriptions from the previous
    // selection. Without this they pile up on long-lived objects/assets, growing
    // the cost of every notify and of SelectionManager's Unsubscribe scan.
    for (auto& [obs, subId] : m_inspectorSubs)
        obs->Unsubscribe(subId);
    m_inspectorSubs.clear();

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
        auto* content = visitor.GetContent();
        if (content) objectHeader->AddWidget(content);
        contentLayout->addWidget(objectHeader);

        // Rebuild whenever a component is added or removed on this object (Add
        // Component button, or a component header's delete menu). Torn down with the
        // rest of m_inspectorSubs on the next rebuild.
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
        }

        for (auto* comp : obj->GetAllComponents()) {
            InspectorVisitor compVisitor;
            comp->Accept(&compVisitor);
            if (!compVisitor.HasContent()) continue;
            content = compVisitor.GetContent();
            ComponentHeader* compWidget = new ComponentHeader(comp->GetTypeName());
            compWidget->Bind(comp->GetID());
            for (auto& s : compVisitor.GetSubscriptions()) m_inspectorSubs.push_back(s);
            for (auto& s : compWidget->GetSubscriptions())  m_inspectorSubs.push_back(s);
            compWidget->AddWidget(content);
            contentLayout->addWidget(compWidget);

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
                if (!obj->HasComponentByName(name))
                    items.push_back(name);

            auto* picker = new ComponentPickerWidget(std::move(items), this);
            picker->setFixedWidth(addComponentButton->width());
            picker->onSelected = [this, obj](const std::string& typeName) {
                // component_ids is keyed by type name, so a duplicate would silently
                // orphan the previous component's registry entry — guard against it.
                if (obj->HasComponentByName(typeName)) return;
                auto* comp = dynamic_cast<Component*>(SerializableFactory::Create(typeName));
                if (!comp) return;
                obj->AddComponent(comp);  // fires ADD_COMPONENT_EVENT → inspector rebuilds
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
            m_materialShaderSubs.emplace_back(mat, subId);
        }
    }

    scrollArea->setWidget(contentWidget);
    
}