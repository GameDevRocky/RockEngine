#include "engine/components/Transform.hpp"
#include <glm/gtc/matrix_inverse.hpp>
#include "engine/serialization/Registry.hpp"
#include "engine/debug/Console.hpp"
#include "Engine.hpp"

void Transform::Init(){
    if (state >= State::Initialized) return; 
    registry = container->FindSystem<Registry>();
    if (parent_id.empty()){
        SetParent(nullptr);
        return;
    }
    Transform* parentTransform = registry->Find<Transform>(parent_id);
    if (parentTransform) SetParent(parentTransform);
    MarkDirty();
    state = State::Initialized; 
}

void Transform::PostInit(){
    if (state >= State::PostInitialized) return;
    
    Console::Comment("Transform::PostInit called for " + GetID() + ", parent_id: '" + parent_id + "'");
  
    state = State::PostInitialized; 
}

void Transform::MarkDirty() {
    dirty = true;
    Engine* engine = Engine::Get();
    Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();

    for (const std::string& id : children_ids) {
        Transform* transform = registry->Find<Transform>(id);
        if (transform) transform->MarkDirty();
    }
}

void Transform::Translate(const glm::vec2& delta) {
    localPosition += delta;
    MarkDirty();
    Notify(POSITION_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void Transform::Rotate(float deltaDeg) {
    localRotation += deltaDeg;
    MarkDirty();
    Notify(ROTATION_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void Transform::Scale(const glm::vec2& delta) {
    localScale += delta;
    MarkDirty();
    Notify(SCALE_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}


void Transform::SetPosition(const glm::vec2& pos) {
    localPosition = pos;
    MarkDirty();
    Notify(POSITION_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void Transform::SetRotation(float degrees) {
    localRotation = degrees;
    MarkDirty();
    Notify(ROTATION_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void Transform::SetScale(const glm::vec2& scale) {
    localScale = scale;
    MarkDirty();
    Notify(SCALE_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

glm::vec2 Transform::GetWorldPosition() const {
    glm::mat4 world = GetWorldMatrix();
    return glm::vec2(world[3]);
}

float Transform::GetWorldRotation() const {
    glm::mat4 world = GetWorldMatrix();
    return glm::degrees(atan2(world[0][1], world[0][0]));
}

glm::vec2 Transform::GetWorldScale() const {
    glm::mat4 world = GetWorldMatrix();
    return glm::vec2(glm::length(glm::vec2(world[0])), glm::length(glm::vec2(world[1])));
}

void Transform::SetWorldPosition(const glm::vec2& pos) {
    Transform* parent = GetParent();
    if (parent) {
        glm::mat4 parentWorld = parent->GetWorldMatrix();
        glm::vec4 localPos = glm::inverse(parentWorld) * glm::vec4(pos, 0.0f, 1.0f);
        localPosition = glm::vec2(localPos);
    } else {
        localPosition = pos;
    }
    MarkDirty();
    Notify(POSITION_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void Transform::SetWorldRotation(float degrees) {
    Transform* parent = GetParent();
    if (parent) {
        float parentRot = parent->GetWorldRotation();
        localRotation = degrees - parentRot;
    } else {
        localRotation = degrees;
    }
    MarkDirty();
    Notify(ROTATION_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void Transform::SetWorldScale(const glm::vec2& scale) {
    Transform* parent = GetParent();
    if (parent) {
        glm::vec2 parentScale = parent->GetWorldScale();
        localScale = scale / parentScale;
    } else {
        localScale = scale;
    }
    MarkDirty();
    Notify(SCALE_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
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
    glm::mat4 oldWorld = GetWorldMatrix();

    if (!parent_id.empty()) {
        
        Transform* oldParent = registry->Find<Transform>(parent_id);
        if (oldParent) {
            auto& vec = oldParent->children_ids;
            vec.erase(std::remove(vec.begin(), vec.end(), GetID()), vec.end());
        }
    }
    if (newParent) {
        parent_id = newParent->GetID();
        newParent->children_ids.push_back(GetID());

    }
    else {
        parent_id.clear();
    }

    if (keepWorld) {
        glm::mat4 parentWorld = (newParent ? newParent->GetWorldMatrix() : glm::mat4(1.0f));
        glm::mat4 newLocal = glm::inverse(parentWorld) * oldWorld;

        localPosition = glm::vec2(newLocal[3]);
        localRotation = glm::degrees(atan2(newLocal[0][1], newLocal[0][0]));
        localScale = glm::vec2(glm::length(glm::vec2(newLocal[0])),
                               glm::length(glm::vec2(newLocal[1])));
    }

    MarkDirty();
    Notify(PARENT_CHANGED_EVENT, this->parent_id);
    Notify(CHANGED_EVENT);
}


std::vector<Transform*> Transform::GetChildren() {
    std::vector<Transform*> result;
    result.reserve(children_ids.size());

    if (!registry) {
        Console::Alert("Transform::GetChildren called but registry is null (not initialized?)");
        return result;
    }

    for (const std::string& id : children_ids) {
        Transform* child = registry->Find<Transform>(id);
        if (child) result.push_back(child);
    }

    return result;
}


Transform* Transform::GetParent() {
    if (parent_id.empty())
        return nullptr;
    
    if (!registry) {
        Console::Alert("Transform::GetParent called but registry is null (not initialized?)");
        return nullptr;
    }
    
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
    if (!node["parent_id"].IsNull()) {
        parent_id = node["parent_id"].as<std::string>();
    } else {
        parent_id = "";
    }
    children_ids.clear();
    
    localPosition.x = node["localPosition"][0].as<float>();
    localPosition.y = node["localPosition"][1].as<float>();
    localRotation = node["localRotation"].as<float>();
    localScale.x = node["localScale"][0].as<float>();
    localScale.y = node["localScale"][1].as<float>();
    MarkDirty();
}

Transform* Transform::Copy() {

    Transform* copy = new Transform();
    copy->id = id;
    copy->enabled = enabled;
    copy->gameobject_id = gameobject_id;
    copy->parent_id = parent_id;
    copy->children_ids = children_ids;
    copy->localPosition = localPosition;
    copy->localRotation = localRotation;
    copy->localScale= localScale;
    state = State::Loaded;
    return copy;
}
