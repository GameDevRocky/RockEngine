#include "engine/serialization/SerializableFactory.hpp"

void SerializableFactory::RegisterType(const std::string& name, Creator creator) {
    GetRegistry()[name] = creator;
}

Serializable* SerializableFactory::Create(const std::string& name) {
    auto it = GetRegistry().find(name);
    if (it != GetRegistry().end())
        return it->second(); // assuming creator returns Serializable*
    return nullptr;
}
