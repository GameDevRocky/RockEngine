#include "engine/serialization/Registry.hpp"
#include "engine/serialization/Serializable.hpp" 
#include <algorithm>
#include "engine/debug/Console.hpp"
#include "engine/core/RuntimeObject.hpp"

void Registry::Register(RuntimeObject* obj) {
    if (!obj) return;
    auto result = runtimeObjects.insert({obj->GetID(), obj});

    if (!result.second) {
        printf("Warning: Object with ID %s already registered. Skipping.\n", obj->GetID().c_str());
    }
}
void Registry::Unregister(RuntimeObject* obj) {
    if (!obj) return;
    auto it = std::find_if(runtimeObjects.begin(), runtimeObjects.end(),
        [&](auto& pair) { return pair.second == obj; });
    if (it != runtimeObjects.end())
        runtimeObjects.erase(it);
}

Registry* Registry::Copy(){

    Registry* registry = new Registry();
    for (auto& pair : runtimeObjects){
        auto* obj = pair.second;

        auto* copy = obj->Copy();
        registry->Register(copy);
    }
    return registry;
}

Registry* Registry::Copy(Container* container) {
    Registry* copy = this->Copy();
    for (auto& kv : copy->runtimeObjects){
        RuntimeObject* obj = kv.second;
        obj->Attach(container);
    }
    copy->Attach(container);
    return copy;
}
