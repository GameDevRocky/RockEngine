#include "engine/serialization/SerializableFactory.hpp"
#include <iostream>
#include <algorithm>
void SerializableFactory::RegisterType(const std::string& name, Creator creator) {
    auto &reg = GetRegistry();
    auto it = reg.find(name);
    if (it == reg.end()) {
        reg[name] = creator;
        std::cout << "Register TYPE: " << name << std::endl;
    } else {
        // Already registered — ignore duplicate registration to avoid noisy logs
    }
}

Serializable* SerializableFactory::Create(const std::string& name) {
    auto it = GetRegistry().find(name);
    if (it != GetRegistry().end())
        return it->second(); // assuming creator returns Serializable*
    return nullptr;
}

std::vector<std::string> SerializableFactory::GetRegisteredTypeNames() {
    auto& reg = GetRegistry();
    std::vector<std::string> names;
    names.reserve(reg.size());
    for (const auto& [name, creator] : reg)
        names.push_back(name);
    std::sort(names.begin(), names.end());
    return names;
}
