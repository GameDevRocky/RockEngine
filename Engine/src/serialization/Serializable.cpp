#include "engine/serialization/Serializable.hpp"
#include "engine/serialization/Registry.hpp"

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
