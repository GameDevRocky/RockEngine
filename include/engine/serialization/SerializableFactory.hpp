#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
 
class Serializable;

class SerializableFactory {
public:
    using Creator = std::function<std::shared_ptr<Serializable>()>;

    static void RegisterType(const std::string& name, Creator creator);
    
    static std::shared_ptr<Serializable> Create(const std::string& name);

private:
    static std::unordered_map<std::string, Creator>& GetRegistry() {
        static std::unordered_map<std::string, Creator> registry;
        return registry;
    }
};


#define REGISTER_SERIALIZABLE_TYPE(TYPE) \
    static struct TYPE##Registrar { \
        TYPE##Registrar() { \
            SerializableFactory::RegisterType(#TYPE, []() { return std::make_shared<TYPE>(); }); \
        } \
    } TYPE##RegistrarInstance;
