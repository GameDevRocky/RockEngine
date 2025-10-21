#pragma once
#include "engine/components/Component.hpp"
#include "engine/serialization/SerializableFactory.hpp"
#include "yaml-cpp/yaml.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> // for translate, rotate, scale
#include <glm/gtc/type_ptr.hpp>         // for sending to OpenGL

class Transform : public Component {
public:
    glm::vec2 position = {0.0f, 0.0f};
    float rotation = 0.0f;
    glm::vec2 scale = {1.0f, 1.0f};

    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;
    void PostDeserialize() override;
    std::string GetTypeName() const override { return "Transform"; }

    void Translate(glm::vec2 translation) {
        position += translation;
    }

    glm::mat4 GetTransformMatrix() const;
};

REGISTER_SERIALIZABLE_TYPE(Transform)
