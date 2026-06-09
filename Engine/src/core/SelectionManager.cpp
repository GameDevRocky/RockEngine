#include "engine/core/SelectionManager.hpp"
#include "engine/core/Container.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/rendering/core/AssetManager.hpp"

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

Serializable* SelectionManager::GetSerializable() const {
    if (selectedObjectId.empty()) return nullptr;

    // Check runtime objects first (GameObjects, Components)
    auto* reg = container->FindSystem<Registry>();
    if (reg) {
        auto* obj = reg->Find<Serializable>(selectedObjectId);
        if (obj) return obj;
    }

    // Fall back to AssetManager
    auto& am = AssetManager::Get();
    if (auto* s = am.GetSprite(selectedObjectId))   return s;
    if (auto* m = am.GetMaterial(selectedObjectId)) return m;
    if (auto* t = am.GetTexture(selectedObjectId))  return t;
    if (auto* sh = am.GetShader(selectedObjectId))  return sh;

    return nullptr;
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

    // Try to find as a GameObject first — subscribe to its shutdown so we
    // auto-deselect when it's destroyed.
    auto* obj = registry->Find<GameObject>(selectedObjectId);
    if (obj) {
        m_shutdownSubId = obj->Subscribe([](std::any data){
            auto* sm = Engine::Get()->GetActiveContainer()->FindSystem<SelectionManager>();
            sm->Deselect();
            return false;
        }, RuntimeObject::SHUTDOWN_EVENT);
        Notify(SELECTION_CHANGED_EVENT, selectedObjectId);
        return;
    }

    // Fall back: check AssetManager. Assets don't have a shutdown event,
    // so just verify the ID exists then notify.
    auto& am = AssetManager::Get();
    bool found = am.GetSprite(selectedObjectId)   != nullptr
              || am.GetMaterial(selectedObjectId) != nullptr
              || am.GetTexture(selectedObjectId)  != nullptr
              || am.GetShader(selectedObjectId)   != nullptr;

    if (!found) {
        selectedObjectId.clear();
        return;
    }

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
