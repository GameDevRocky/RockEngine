#include "engine/serialization/Serializable.hpp"
#include "engine/serialization/Registry.hpp"

Serializable::Serializable(){
    Registry::Get().Register(this);
}
Serializable::~Serializable(){
    Registry::Get().Unregister(this);
}

YAML::Node Serializable::Serialize() {
    YAML::Node node;
    node["id"] = id;
    node["type"] = GetTypeName();
    return node;
}

void Serializable::Deserialize(const YAML::Node& node) {
    if (node["id"])
        id = node["id"].as<std::string>();
}