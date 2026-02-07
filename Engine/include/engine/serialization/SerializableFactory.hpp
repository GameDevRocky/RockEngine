#pragma once
#include <string>
#include <unordered_map>
#include <functional>

class Serializable;

class SerializableFactory {
public:
    using Creator = std::function<Serializable*()>;

    static void RegisterType(const std::string& name, Creator creator);
    
    static Serializable* Create(const std::string& name);

private:
    static std::unordered_map<std::string, Creator>& GetRegistry() {
        static std::unordered_map<std::string, Creator> registry;
        return registry;
    }
};

