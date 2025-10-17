#pragma once
#include "engine/core/Component.hpp"
#include "engine/serialization/SerializableFactory.hpp"
#include "yaml-cpp/yaml.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> // for translate, rotate, scale
#include <glm/gtc/type_ptr.hpp>         // for sending to OpenGL

class Transform : public Component {
public:
    glm::vec2 position = {100.0f, 50.0f};
    float rotation = 0.0f; // in degrees
    glm::vec2 scale = {1.0f, 1.0f};

    YAML::Node Serialize() override;

    void Deserialize(const YAML::Node& node) override;
    std::string GetTypeName() const override { return "Transform"; }

  
    glm::mat4 GetTransformMatrix() const;
};

REGISTER_SERIALIZABLE_TYPE(Transform)
