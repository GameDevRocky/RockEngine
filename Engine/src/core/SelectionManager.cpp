#include "engine/core/SelectionManager.hpp"
#include "engine/core/Container.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/serialization/Registry.hpp"

void SelectionManager::Init()
{
    registry = container->FindSystem<Registry>();
    selectedObjectId.clear();
}

void SelectionManager::Shutdown()
{
    selectedObjectId.clear();
}

SelectionManager* SelectionManager::Copy()
{
    auto* copy = new SelectionManager();
    copy->selectedObjectId = selectedObjectId;
    copy->subscribers = subscribers;
    return copy;
}

SelectionManager* SelectionManager::Copy(Container* container)
{
    auto* copy = new SelectionManager();
    copy->Attach(container);
    return copy;
}

GameObject* SelectionManager::GetGameObject(){
    if (selectedObjectId.empty()) return nullptr;
    auto* registry = container->FindSystem<Registry>();
    auto* obj = registry->Find<GameObject>(selectedObjectId);  
    return obj;
}

void SelectionManager::Select(const std::string& objectId)
{
    if (selectedObjectId == objectId) return;
    
    selectedObjectId = objectId;
    auto* obj = registry->Find<GameObject>(selectedObjectId);  
    if (!obj){
        Deselect();
        return;
    }

    obj->Subscribe([](std::any data){
        auto* sm = Engine::Get()->GetActiveContainer()->FindSystem<SelectionManager>();
        sm->Deselect();
        return true;
    }, RuntimeObject::SHUTDOWN_EVENT);

    Notify(SELECTION_CHANGED_EVENT, selectedObjectId);
}

void SelectionManager::Deselect()
{
    if (selectedObjectId.empty()) return;
    
    selectedObjectId.clear();
    Notify(SELECTION_CHANGED_EVENT, selectedObjectId);
}
