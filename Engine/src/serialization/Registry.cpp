#include "engine/serialization/Registry.hpp"
#include "engine/serialization/Serializable.hpp" 
#include <algorithm>
#include "engine/debug/Console.hpp"
#include "engine/core/RuntimeObject.hpp"
#include "Engine.hpp"

Registry* Registry::GetRuntimeRegistry() {
    auto* engine = Engine::Get();
    if (!engine) return nullptr;

    auto* container = engine->GetActiveContainer();
    if (!container) return nullptr;

    return container->FindSystem<Registry>();
}

void Registry::Register(RuntimeObject* obj) {
    if (!obj) return;
    std::string id = obj->GetID();
    auto result = runtimeObjects.insert({id, obj});
    obj->Subscribe([id](std::any data){
        auto* registry = Engine::Get()->GetActiveContainer()->FindSystem<Registry>();
        if (!registry) return true;
        auto* obj = registry->Find<RuntimeObject>(id);
        if (!obj) return true;
        registry->runtimeObjects.erase(id);
        registry->pendingDeletes.push_back(obj);
        return true;
    }, RuntimeObject::SHUTDOWN_EVENT);

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

void Registry::Update() {
    for (auto* obj : pendingDeletes)
        delete obj;
    pendingDeletes.clear();
}

void Registry::Shutdown(){
    // Drain any objects already shut down but awaiting deletion.
    for (auto* obj : pendingDeletes)
        delete obj;
    pendingDeletes.clear();

    // Snapshot and clear first so the SHUTDOWN_EVENT lambda is a no-op
    // (avoids erase-during-iteration and double-delete).
    auto objects = runtimeObjects;
    runtimeObjects.clear();
    for (auto& [key, obj] : objects){
        obj->Shutdown();
        delete obj;
    }
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
