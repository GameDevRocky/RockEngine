#include "engine/serialization/Registry.hpp"
#include "engine/serialization/Serializable.hpp" 
#include <algorithm>
#include "engine/debug/Console.hpp"
#include "engine/core/RuntimeObject.hpp"

void Registry::Init(){
    std::cout << "Registry Initialized" << std::endl;
    
}
void Registry::PostInit(){
    std::cout << "Registry Post Initialized" << std::endl;

}
void Registry::Shutdown(){

}

void Registry::Register(Serializable* obj) {
    if (!obj) return;

    std::string id = obj->GetID();
    serializables[id] = obj;
}
 
void Registry::Unregister(Serializable* obj) {
    if (!obj) return;
    auto it = std::find_if(serializables.begin(), serializables.end(),
        [&](auto& pair) { return pair.second == obj; });
    if (it != serializables.end())
        serializables.erase(it);
}

Registry* Registry::Copy(){

    Registry* registry = new Registry();
    for (auto& pair : serializables){
        auto* obj = pair.second;

        auto* copy = obj->Copy();
        registry->Register(copy);
    }
    return registry;
}

Registry* Registry::Copy(Container* container) {
    Registry* registry = new Registry();

    for (auto& pair : serializables){
        auto* obj = pair.second;

        if (auto* runtimeObj = dynamic_cast<RuntimeObject*>(obj)) {
            auto* runtimeCopy = runtimeObj->Copy(container);
            auto* serializableCopy = dynamic_cast<Serializable*>(runtimeCopy);
            if (serializableCopy){
                std::cout << "Copying Object" + serializableCopy->GetTypeName() << std::endl;
                registry->Register(serializableCopy);
            }
            continue;
        }

        auto* copy = obj->Copy();
        registry->Register(copy);
    }

    registry->Attach(container);

    return registry;
}
