#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include <vector>

class Serializable;

class SerializableFactory {
public:
    using Creator = std::function<Serializable*()>;

    static void RegisterType(const std::string& name, Creator creator);

    static Serializable* Create(const std::string& name);

    // Names of every registered type, sorted alphabetically. Used by the editor to
    // populate the "Add Component" picker.
    static std::vector<std::string> GetRegisteredTypeNames();

private:
    static std::unordered_map<std::string, Creator>& GetRegistry() {
        static std::unordered_map<std::string, Creator> registry;
        return registry;
    }
};

