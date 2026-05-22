#pragma once
#include <unordered_map>
#include <string>
#include <vector>
#include <functional>
#include <iostream>
#include "engine/core/System.hpp"
class RuntimeObject;

class Registry : public System {
private:
    std::unordered_map<std::string, RuntimeObject*> runtimeObjects;
    std::vector<RuntimeObject*> pendingDeletes;
    static Registry* GetRuntimeRegistry();

public:

    Registry() = default;
    void Update() override;
    void Shutdown() override;
    Registry* Copy() override;
    Registry* Copy(Container* container) override;
    
    void Register(RuntimeObject* obj);
    void Unregister(RuntimeObject* obj);

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


    std::unordered_map<std::string, RuntimeObject*>& GetAll() { return runtimeObjects; }
};
