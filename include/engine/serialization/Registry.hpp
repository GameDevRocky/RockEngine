#pragma once
#include <unordered_map>
#include <string>

class Serializable;

class Registry {
private:
    std::unordered_map<std::string, Serializable*> objects;

public:

    static Registry& Get() {
        static Registry instance;
        return instance;
    }
    void Register(Serializable* obj);
    void Unregister(Serializable* obj);
    Serializable* Get(const std::string& id);
};
