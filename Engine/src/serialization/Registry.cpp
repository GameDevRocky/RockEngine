#include "engine/serialization/Registry.hpp"
#include "engine/serialization/Serializable.hpp" 
#include <algorithm>
#include "engine/debug/Console.hpp"

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


