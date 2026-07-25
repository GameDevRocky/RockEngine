#pragma once
#include <unordered_map>
#include <string>
#include <vector>
#include <functional>
#include <iostream>
#include <atomic>
#include <cstdint>
#include "engine/core/System.hpp"

class RuntimeObject;

class Registry : public System {
private:
    std::unordered_map<std::string, RuntimeObject*> runtimeObjects;
    std::vector<RuntimeObject*> pendingShutdowns;
    std::vector<RuntimeObject*> pendingDeletes;
    static Registry* GetRuntimeRegistry();

    // A single process-wide counter, bumped on every structural change that
    // could invalidate a resolved pointer (object register/unregister/destroy,
    // and reparent/reorder/add/remove that shuffle id lists without touching
    // the map — see BumpGeneration callers). Per-frame pointer caches stamp
    // themselves with this and re-resolve when it moves. Global, not
    // per-registry, so an editor-container cache can never falsely validate
    // against the runtime registry after the play-mode swap.
    static inline std::atomic<std::uint64_t> s_generation{1};

public:

    Registry() = default;
    void Update() override;
    void Shutdown() override;
    Registry* Copy() override;
    Registry* Copy(Container* container) override;

    // Read the current generation (relaxed: caches only need a value that
    // changes, not cross-thread ordering — everything runs on the GUI thread).
    static std::uint64_t Generation() { return s_generation.load(std::memory_order_relaxed); }
    // Invalidate every pointer cache. Registry lifecycle calls this itself; the
    // few structural mutators outside the registry (Transform reparent/reorder,
    // Scene root/object list syncs) call it explicitly.
    static void BumpGeneration() { s_generation.fetch_add(1, std::memory_order_relaxed); }

    void Register(RuntimeObject* obj);
    void Unregister(RuntimeObject* obj);
    void Destroy(RuntimeObject* obj);
    void FlushPendingShutdowns();

    template<typename T = RuntimeObject> 
    T* Find(const std::string& id) {
        auto it = runtimeObjects.find(id);
        if (it != runtimeObjects.end()) {
            return dynamic_cast<T*>(it->second);
        }
        return nullptr;
    }
    template<typename T = RuntimeObject> 
    static T* FindInRuntime(const std::string& id) {
        auto* registry = GetRuntimeRegistry();
        if (!registry) return nullptr;
        auto it = registry->runtimeObjects.find(id);
        if (it != registry->runtimeObjects.end()) {
            return dynamic_cast<T*>(it->second);
        }
        return nullptr;
    }
private:

    std::unordered_map<std::string, RuntimeObject*>& GetAll() { return runtimeObjects; }
};
