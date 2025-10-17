#include "engine/serialization/Registry.hpp"
#include "engine/serialization/Serializable.hpp" // ✅ required for GetID()
#include <algorithm>

void Registry::Register(Serializable* obj) {
    if (!obj) return;

    std::string id = obj->GetID();
    serializables[id] = obj;

    // Try to resolve any pending link requests that target this object
    auto it = std::remove_if(deferredLinks.begin(), deferredLinks.end(),
        [&](LinkRequest& req) {
            if (req.targetUUID == id) {
                req.setter(obj);
                return true; // remove from deferred list
            }
            return false;
        });

    if (it != deferredLinks.end())
        deferredLinks.erase(it, deferredLinks.end());
}

void Registry::Unregister(Serializable* obj) {
    if (!obj) return;
    auto it = std::find_if(serializables.begin(), serializables.end(),
        [&](auto& pair) { return pair.second == obj; });
    if (it != serializables.end())
        serializables.erase(it);
}

Serializable* Registry::Get(const std::string& id) {
    auto it = serializables.find(id);
    return (it != serializables.end()) ? it->second : nullptr;
}

void Registry::DeferLink(const std::string& targetUUID, std::function<void(Serializable*)> setter) {
    // If object is already registered, resolve immediately
    if (auto* obj = Get(targetUUID)) {
        setter(obj);
        return;
    }

    // Otherwise, defer until that ID is registered
    deferredLinks.push_back({setter, targetUUID});
}

void Registry::ResolveLinks() {
    if (deferredLinks.empty()) {
        std::cout << "✅ All links resolved.\n";
        return;
    }

    std::cerr << "⚠️ Unresolved links remaining:\n";
    for (auto& link : deferredLinks) {
        std::cerr << "  - Missing target UUID: " << link.targetUUID << std::endl;
    }
    deferredLinks.clear();
}
