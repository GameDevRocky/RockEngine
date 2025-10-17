#pragma once
#include <string>
#include <unordered_map>
#include <functional>

class Serializable;

class SerializableFactory {
public:
    using Creator = std::function<Serializable*()>;  // Creator returns raw pointer

    static void RegisterType(const std::string& name, Creator creator);
    static Serializable* Create(const std::string& name);

private:
    static std::unordered_map<std::string, Creator>& GetRegistry() {
        static std::unordered_map<std::string, Creator> registry;
        return registry;
    }
};

// Fixed macro to use `new` instead of `make_shared`
#define REGISTER_SERIALIZABLE_TYPE(TYPE) \
    static struct TYPE##Registrar { \
        TYPE##Registrar() { \
            SerializableFactory::RegisterType(#TYPE, []() { return new TYPE(); }); \
        } \
    } TYPE##RegistrarInstance;
