#include "engine/core/SelectionManager.hpp"
#include "engine/core/Container.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/serialization/Registry.hpp"

void SelectionManager::Init()
{
    selectedObjectId.clear();
}

void SelectionManager::Shutdown()
{
    selectedObjectId.clear();
}

SelectionManager* SelectionManager::Copy()
{
    return new SelectionManager();
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
    Notify(SELECTION_CHANGED_EVENT, selectedObjectId);
}

void SelectionManager::Deselect()
{
    if (selectedObjectId.empty()) return;
    
    selectedObjectId.clear();
    Notify(SELECTION_CHANGED_EVENT, selectedObjectId);
}
