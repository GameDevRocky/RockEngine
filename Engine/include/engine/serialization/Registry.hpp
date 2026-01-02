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

    void Register(Serializable* obj);

    void Unregister(Serializable* obj);

    template<typename T = Serializable> 
    static T* Find(const std::string& id) {
        auto it = Get().serializables.find(id);
        if (it != Get().serializables.end()) {
            // dynamic_cast ensures the Serializable* is actually a T*
            return dynamic_cast<T*>(it->second);
        }
        return nullptr;
    }

    void DeferLink(const std::string& targetUUID, std::function<void(Serializable*)> setter);

    void ResolveLinks();

    // Optional helper
    std::unordered_map<std::string, Serializable*>& GetAll() { return serializables; }
};
