#pragma once
#include "engine/components/Component.hpp"
#include "engine/serialization/SerializableFactory.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <yaml-cpp/yaml.h>

class Registry;

class Transform : public Component {
public:
    static inline const Event POSITION_CHANGED_EVENT = Observable::CreateEvent();
    static inline const Event ROTATION_CHANGED_EVENT = Observable::CreateEvent();
    static inline const Event SCALE_CHANGED_EVENT = Observable::CreateEvent();
    static inline const Event PARENT_CHANGED_EVENT = Observable::CreateEvent();

    glm::vec2 localPosition = {0.0f, 0.0f};
    float localRotation = 0.0f;
    glm::vec2 localScale = {1.0f, 1.0f};

    mutable bool dirty = true;
    mutable glm::mat4 worldMatrix = glm::mat4(1.0f);

    Transform* Copy() override;
    void Init() override;
    void PostInit() override;

    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;
    std::string GetTypeName() const override { return "Transform"; }
    
    // Local Setters
    void SetPosition(const glm::vec2& pos);
    void SetRotation(float degrees);
    void SetScale(const glm::vec2& scale);

    // World Getters
    glm::vec2 GetWorldPosition() const;
    float GetWorldRotation() const;
    glm::vec2 GetWorldScale() const;

    // World Setters
    void SetWorldPosition(const glm::vec2& pos);
    void SetWorldRotation(float degrees);
    void SetWorldScale(const glm::vec2& scale);

    // Transformations
    void Translate(const glm::vec2& delta);
    void Rotate(float deltaDeg);
    void Scale(const glm::vec2& delta);
    
    void SetParent(Transform* newParent, bool keepWorld = true);
    Transform* GetParent();
    std::vector<Transform*> GetChildren();
    
    glm::mat4 GetLocalMatrix() const;
    glm::mat4 GetWorldMatrix() const;
    
private:
    void MarkDirty();
    std::string parent_id;
    std::vector<std::string> children_ids;
    Registry* registry;
};