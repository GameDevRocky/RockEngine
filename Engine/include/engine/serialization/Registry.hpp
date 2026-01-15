#pragma once
#include <unordered_map>
#include <string>
#include <vector>
#include <functional>
#include <iostream>
#include "engine/core/System.hpp"

class Serializable;

class Registry : public System{
private:
    std::unordered_map<std::string, Serializable*> serializables;

public:

    Registry() = default;

    void Init() override;
    void PostInit() override;
    void Shutdown() override;
    Registry* Copy() override;
    
    void Register(Serializable* obj);
    void Unregister(Serializable* obj);

    template<typename T = Serializable> 
    T* Find(const std::string& id) {
        auto it = serializables.find(id);
        if (it != serializables.end()) {
            return dynamic_cast<T*>(it->second);
        }
        return nullptr;
    }
    std::unordered_map<std::string, Serializable*>& GetAll() { return serializables; }
};
