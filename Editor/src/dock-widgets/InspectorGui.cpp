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
    engine->Subscribe([this](){
        SubscribeToSelector();
        return true;
    }, Engine::ENTER_PLAY_MODE_EVENT);
    engine->Subscribe([this](){
        SubscribeToSelector();
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

    if (contentWidget) {
        contentWidget->deleteLater();
        contentWidget = nullptr;
    }

    if (id.empty()) return;
    auto* obj = Registry::FindInRuntime<GameObject>(id);
    if (!obj) return;

    contentWidget = new QWidget();
    QVBoxLayout* contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setAlignment(Qt::AlignTop);

    InspectorVisitor visitor;
    ObjectHeader* objectHeader = new ObjectHeader();
    obj->Accept(&visitor);
    objectHeader->Bind(obj->GetID());
    auto* content = visitor.GetContent();
    if (content){
        objectHeader->AddWidget(content);
    }
    contentLayout->addWidget(objectHeader);

    for (auto* comp : obj->GetAllComponents()){
        InspectorVisitor compVisitor;
        comp->Accept(&compVisitor);
        if (!compVisitor.HasContent()) continue;
        content = compVisitor.GetContent();
        ComponentHeader* compWidget = new ComponentHeader(comp->GetTypeName());
        compWidget->Bind(comp->GetID());
        compWidget->AddWidget(content);
        contentLayout->addWidget(compWidget);

        if (auto* sc = dynamic_cast<ScriptComponent*>(comp)) {
            std::string capturedId = id;
            int subId = sc->Subscribe([this, capturedId](std::any) {
                OnObjectSelected(capturedId);
                return true;
            }, ScriptComponent::SCRIPT_RELOADED_EVENT);
            m_scriptReloadSubs.emplace_back(sc->GetID(), subId);
        }
    }

    scrollArea->setWidget(contentWidget);
    
}