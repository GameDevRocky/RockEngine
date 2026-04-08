#include "dock-widgets/InspectorGui.hpp"

#include "Engine.hpp"
#include "engine/core/SelectionManager.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/core/GameObject.hpp"
#include "utils/CollapsableWidget.hpp"

namespace {
    void clearLayout(QLayout* layout)
    {
        if (!layout) return;

        QLayoutItem* item;
        while ((item = layout->takeAt(0)) != nullptr)
        {
            if (QWidget* widget = item->widget())
            {
                widget->deleteLater();
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
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    setLayout(mainLayout);
}

void InspectorGui::Init(){ 
    auto* engine = Engine::Get();
    engine->Subscribe([this](){
        SubscribeToSelector();
    }, Engine::ENTER_PLAY_MODE_EVENT);
    engine->Subscribe([this](){
        SubscribeToSelector();
    }, Engine::EXIT_PLAY_MODE_EVENT);
    SubscribeToSelector();
    std::cout << "InspectorGui Initialized" << std::endl;
}

void InspectorGui::SubscribeToSelector(){
    clearLayout(layout());
    auto* selectionManager = Engine::Get()->GetActiveContainer()->FindSystem<SelectionManager>();
    selectionManager->Subscribe([this](std::any data){
        const std::string& id = std::any_cast<std::string>(data);
        OnObjectSelected(id);
    }, SelectionManager::SELECTION_CHANGED_EVENT);
    OnObjectSelected(selectionManager->GetSelectedId());
}

void InspectorGui::OnObjectSelected(const std::string& id)
{
    clearLayout(layout());
    if (id.empty()) return;
    auto* obj = Registry::FindInRuntime<GameObject>(id);
    if (!obj) return;
    ObjectHeader* header = new ObjectHeader(this);
    header->Bind(id);
    mainLayout->addWidget(header);
    mainLayout->addStretch();
    
}