#include "engine/core/Component.hpp"
#include "engine/serialization/SerializableFactory.hpp"
#include "yaml-cpp/yaml.h"

class Transform : public Component {
public:
    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node node);
    std::string GetTypeName() const override { return "Transform"; }
};

REGISTER_SERIALIZABLE_TYPE(Transform)
