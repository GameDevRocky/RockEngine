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



    // Optional helper
    std::unordered_map<std::string, Serializable*>& GetAll() { return serializables; }
};
