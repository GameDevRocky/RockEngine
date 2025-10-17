#include "engine/serialization/Registry.hpp"
#include "engine/serialization/Serializable.hpp"


void Registry::Register(Serializable* obj) {
    objects[obj->GetID()] = obj;
}

void Registry::Unregister(Serializable* obj) {
    objects.erase(obj->GetID());
}

Serializable* Registry::Get(const std::string& id) {
    auto it = objects.find(id);
    return it != objects.end() ? it->second : nullptr;
}


