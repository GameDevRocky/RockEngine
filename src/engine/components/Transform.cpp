#include "engine/components/Transform.hpp"
#include <glm/gtc/matrix_inverse.hpp>

// ============================================================================
// MARK DIRTY — when any local value changes
// ============================================================================
void Transform::MarkDirty() {
    dirty = true;

    for (Transform* c : children)
        c->MarkDirty();
}

// ============================================================================
// LOCAL MANIPULATION
// ============================================================================
void Transform::Translate(const glm::vec2& delta) {
    localPosition += delta;
    MarkDirty();
}

void Transform::Rotate(float deltaDeg) {
    localRotation += deltaDeg;
    MarkDirty();
}

// ============================================================================
// MATRIX GENERATION
// ============================================================================
glm::mat4 Transform::GetLocalMatrix() const {
    glm::mat4 m(1.0f);
    m = glm::translate(m, glm::vec3(localPosition, 0.0f));
    m = glm::rotate(m, glm::radians(localRotation), glm::vec3(0, 0, 1));
    m = glm::scale(m, glm::vec3(localScale, 1.0f));
    return m;
}

glm::mat4 Transform::GetWorldMatrix() const {
    if (!dirty)
        return worldMatrix;

    if (parent)
        worldMatrix = parent->GetWorldMatrix() * GetLocalMatrix();
    else
        worldMatrix = GetLocalMatrix();

    dirty = false;
    return worldMatrix;
}

// ============================================================================
// PARENTING
// ============================================================================
void Transform::SetParent(Transform* newParent, bool keepWorld) {
    glm::mat4 oldWorld = GetWorldMatrix();

    // Remove from old parent
    if (parent) {
        auto& siblings = parent->children;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
    }

    parent = newParent;
    parent_id = newParent->GetID();

    if (newParent)
        newParent->children.push_back(this);

    if (keepWorld) {
        // Convert world → local
        glm::mat4 newParentWorld = (parent ? parent->GetWorldMatrix() : glm::mat4(1.0f));
        glm::mat4 newLocal = glm::inverse(newParentWorld) * oldWorld;

        // Extract values
        localPosition = glm::vec2(newLocal[3]);
        localRotation = glm::degrees(atan2(newLocal[1][0], newLocal[0][0]));
        localScale = glm::vec2(glm::length(glm::vec2(newLocal[0])),
                               glm::length(glm::vec2(newLocal[1])));
    }

    MarkDirty();
}

// ============================================================================
// SERIALIZATION
// ============================================================================
YAML::Node Transform::Serialize() {
    YAML::Node node;
    node["type"] = GetTypeName();

    node["localPosition"][0] = localPosition.x;
    node["localPosition"][1] = localPosition.y;
    node["localRotation"] = localRotation;
    node["localScale"][0] = localScale.x;
    node["localScale"][1] = localScale.y;
    node["parent_id"] = parent_id;

    return node;
}

void Transform::Deserialize(const YAML::Node& node) {
    Component::Deserialize(node);    
    parent_id = !node["parent_id"].IsNull()?  node["parent_id"].as<std::string>() : "";
    localPosition.x = node["localPosition"][0].as<float>();
    localPosition.y = node["localPosition"][1].as<float>();
    localRotation = node["localRotation"].as<float>();
    localScale.x = node["localScale"][0].as<float>();
    localScale.y = node["localScale"][1].as<float>();
    MarkDirty();
}

void Transform::PostDeserialize() {



}
