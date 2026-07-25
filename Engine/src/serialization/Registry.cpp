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
    BumpGeneration();
    obj->Subscribe([id](std::any data){
        auto* registry = Engine::Get()->GetActiveContainer()->FindSystem<Registry>();
        if (!registry) return true;
        auto it = registry->runtimeObjects.find(id);
        if (it == registry->runtimeObjects.end()) return true;
        registry->pendingDeletes.push_back(it->second);
        registry->runtimeObjects.erase(it);
        // Bump BEFORE the object is deleted (delete happens on the next flush),
        // so any cache holding this now-dangling pointer re-resolves first.
        BumpGeneration();
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
    if (it != runtimeObjects.end()) {
        runtimeObjects.erase(it);
        BumpGeneration();
    }
}

void Registry::Update() {
}

void Registry::Destroy(RuntimeObject* obj) {
    if (!obj) return;
    for (auto* queued : pendingShutdowns)
        if (queued == obj) return;
    obj->MarkForDestroy();
    pendingShutdowns.push_back(obj);
}

void Registry::FlushPendingShutdowns() {
    std::vector<RuntimeObject*> toProcess;
    std::swap(toProcess, pendingShutdowns);
    for (auto* obj : toProcess) {
        if (runtimeObjects.find(obj->GetID()) == runtimeObjects.end()) continue;
        obj->Shutdown();
        // obj itself is now in pendingDeletes (from its SHUTDOWN_EVENT); remove it
        // so we can delete it inline rather than double-freeing in the drain below.
        auto it = std::find(pendingDeletes.begin(), pendingDeletes.end(), obj);
        if (it != pendingDeletes.end()) pendingDeletes.erase(it);
        delete obj;
    }
    // Delete components (and any other children) that were shut down as a
    // side-effect of the above GameObject::Shutdown() bottom-up recursion.
    for (auto* obj : pendingDeletes)
        delete obj;
    pendingDeletes.clear();
    BumpGeneration();
}

void Registry::Shutdown(){
    // Drain any components awaiting deletion from a prior flush.
    for (auto* obj : pendingDeletes)
        delete obj;
    pendingDeletes.clear();

    // Snapshot and clear first so the SHUTDOWN_EVENT lambda is a no-op
    // (avoids erase-during-iteration and double-delete).
    auto objects = runtimeObjects;
    runtimeObjects.clear();
    BumpGeneration();
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
