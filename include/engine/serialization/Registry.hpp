#pragma once
#include <unordered_map>
#include <string>
#include <vector>
#include <functional>
#include <iostream>

class Serializable;

class Registry {
private:
    std::unordered_map<std::string, Serializable*> serializables;

    Registry() = default; // private constructor for Singleton
public:
    // Singleton accessor
    static Registry& Get() {
        static Registry instance;
        return instance;
    }

    struct LinkRequest {
        std::function<void(Serializable*)> setter; 
        std::string targetUUID;                   
    };
    using LinkCallback = std::function<void(Serializable*)>;

    std::vector<LinkRequest> deferredLinks;

    // ✅ Register an object
    void Register(Serializable* obj);

    // ✅ Unregister (optional for scene unloading)
    void Unregister(Serializable* obj);

    // ✅ Retrieve object by UUID
    Serializable* Get(const std::string& id);

    // ✅ Add a deferred link operation
    void DeferLink(const std::string& targetUUID, std::function<void(Serializable*)> setter);

    // ✅ Resolve all deferred links once all objects are created
    void ResolveLinks();

    // Optional helper
    std::unordered_map<std::string, Serializable*>& GetAll() { return serializables; }
};
