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
    m_shutdownSubId = -1;
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

    // Unsubscribe from the previously selected object's shutdown event
    if (m_shutdownSubId != -1) {
        auto* prevObj = registry->Find<GameObject>(selectedObjectId);
        if (prevObj) prevObj->Unsubscribe(m_shutdownSubId);
        m_shutdownSubId = -1;
    }

    selectedObjectId = objectId;
    auto* obj = registry->Find<GameObject>(selectedObjectId);  
    if (!obj){
        Deselect();
        return;
    }

    // Return false so the callback auto-removes itself on fire,
    // avoiding a re-entrant Unsubscribe during SHUTDOWN_EVENT iteration.
    m_shutdownSubId = obj->Subscribe([](std::any data){
        auto* sm = Engine::Get()->GetActiveContainer()->FindSystem<SelectionManager>();
        sm->Deselect();
        return false;
    }, RuntimeObject::SHUTDOWN_EVENT);

    Notify(SELECTION_CHANGED_EVENT, selectedObjectId);
}

void SelectionManager::Deselect()
{
    if (selectedObjectId.empty()) return;

    if (m_shutdownSubId != -1) {
        auto* obj = registry->Find<GameObject>(selectedObjectId);
        if (obj) obj->Unsubscribe(m_shutdownSubId);
        m_shutdownSubId = -1;
    }

    selectedObjectId.clear();
    Notify(SELECTION_CHANGED_EVENT, selectedObjectId);
}
