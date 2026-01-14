#include "engine/components/Transform.hpp"
#include <glm/gtc/matrix_inverse.hpp>
#include "engine/serialization/Registry.hpp"
#include "engine/debug/Console.hpp"
#include "Engine.hpp"

void Transform::MarkDirty() {
    dirty = true;
    Engine* engine = Engine::Get();
    Registry* registry = engine->GetActiveContainer()->GetRegistry();

    for (const std::string& id : children_ids) {
        Transform* transform = registry->Find<Transform>(id);
        if (transform) transform->MarkDirty();
    }
}

void Transform::Translate(const glm::vec2& delta) {
    localPosition += delta;
    MarkDirty();
    Notify();
}

void Transform::Rotate(float deltaDeg) {
    localRotation += deltaDeg;
    MarkDirty();
    Notify();
}

void Transform::Scale(const glm::vec2& delta) {
    localScale += delta;
    MarkDirty();
    Notify();
}


void Transform::SetPosition(const glm::vec2& pos) {
    localPosition = pos;
    MarkDirty();
    Notify();
}

void Transform::SetRotation(float degrees) {
    localRotation = degrees;
    MarkDirty();
    Notify();
}

void Transform::SetScale(const glm::vec2& scale) {
    localScale = scale;
    MarkDirty();
    Notify();
}


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

    Transform* p = const_cast<Transform*>(this)->GetParent();

    if (p)
        worldMatrix = p->GetWorldMatrix() * GetLocalMatrix();
    else
        worldMatrix = GetLocalMatrix();

    dirty = false;
    return worldMatrix;
}


void Transform::SetParent(Transform* newParent, bool keepWorld) {
    Scene* scene = GetGameObject()->GetScene();

    glm::mat4 oldWorld = GetWorldMatrix();

    if (!parent_id.empty()) {
        Engine* engine = Engine::Get();
        Registry* registry = engine->GetActiveContainer()->GetRegistry();
        Transform* oldParent = registry->Find<Transform>(parent_id);
        if (oldParent) {
            auto& vec = oldParent->children_ids;
            vec.erase(std::remove(vec.begin(), vec.end(), GetID()), vec.end());
        }

        if (scene)
            scene->RemoveRootObject(GetGameObject()->GetID());
    }
    if (newParent) {
        parent_id = newParent->GetID();
        newParent->children_ids.push_back(GetID());

        if (scene)
            scene->RemoveRootObject(GetGameObject()->GetID());
    }
    else {
        parent_id.clear();

        if (scene)
            scene->AddRootObject(GetGameObject()->GetID());
    }

    if (keepWorld) {
        glm::mat4 parentWorld = (newParent ? newParent->GetWorldMatrix() : glm::mat4(1.0f));
        glm::mat4 newLocal = glm::inverse(parentWorld) * oldWorld;

        localPosition = glm::vec2(newLocal[3]);
        localRotation = glm::degrees(atan2(newLocal[1][0], newLocal[0][0]));
        localScale = glm::vec2(glm::length(glm::vec2(newLocal[0])),
                               glm::length(glm::vec2(newLocal[1])));
    }

    MarkDirty();
    Notify();
}


std::vector<Transform*> Transform::GetChildren() {
    std::vector<Transform*> result;
    result.reserve(children_ids.size());

    for (const std::string& id : children_ids) {
        Engine* engine = Engine::Get();
        Registry* registry = engine->GetActiveContainer()->GetRegistry();
        Transform* child = registry->Find<Transform>(id);
        if (child) result.push_back(child);
    }

    return result;
}


Transform* Transform::GetParent() {
    if (parent_id.empty())
        return nullptr;
    Engine* engine = Engine::Get();
    Registry* registry = engine->GetActiveContainer()->GetRegistry();
    Transform* transform = registry->Find<Transform>(parent_id);

    if (!transform) {
        Console::Alert("Transform parent_id exists but is not a Transform");
        return nullptr;
    }

    return transform;
}




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
    if (parent_id.empty()) {
        SetParent(nullptr);
        return;
    }

    Engine* engine = Engine::Get();
    Registry* registry = engine->GetActiveContainer()->GetRegistry();
    Transform* transform = registry->Find<Transform>(parent_id);

    if (!transform) {
        Console::Warn("Unable to connect Transform parent");
        return;
    }

    SetParent(transform);
    Console::Comment("Transform parent connected");
}
