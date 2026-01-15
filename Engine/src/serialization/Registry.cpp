#include "engine/serialization/Registry.hpp"
#include "engine/serialization/Serializable.hpp" 
#include <algorithm>
#include "engine/debug/Console.hpp"

void Registry::Init(){
    
}
void Registry::PostInit(){

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
        std::cout << obj->GetTypeName() << std::endl;
        std::cout << obj->GetID() << std::endl;
        auto* copy = obj->Copy();
        registry->Register(copy);
    }
    return registry;
}
