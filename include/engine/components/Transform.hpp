#pragma once
#include "engine/components/Component.hpp"
#include "engine/serialization/SerializableFactory.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <yaml-cpp/yaml.h>

class Transform : public Component {
public:

    // -------- Local Transform --------
    glm::vec2 localPosition = {0.0f, 0.0f};
    float localRotation = 0.0f;     // Degrees
    glm::vec2 localScale = {1.0f, 1.0f};

    // -------- Hierarchy --------
    Transform* parent = nullptr;
    std::vector<Transform*> children;

    // -------- Cached World Matrix --------
    mutable bool dirty = true;
    mutable glm::mat4 worldMatrix = glm::mat4(1.0f);
    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;
    void PostDeserialize() override;
    std::string GetTypeName() const override { return "Transform"; }

    void Translate(const glm::vec2& delta);
    void Rotate(float deltaDeg);
    void SetParent(Transform* newParent, bool keepWorld = true);

    glm::mat4 GetLocalMatrix() const;
    glm::mat4 GetWorldMatrix() const;

private:
    void MarkDirty();

    std::string parent_id;

};
