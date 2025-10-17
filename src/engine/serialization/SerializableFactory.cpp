#include "engine/serialization/SerializableFactory.hpp"


void SerializableFactory::RegisterType(const std::string& name, Creator creator){
    GetRegistry()[name] = creator;
}

std::shared_ptr<Serializable> SerializableFactory::Create(const std::string& name) {
        auto it = GetRegistry().find(name);
        if (it != GetRegistry().end())
            return it->second();
        return nullptr;
    }
