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
    state = State::PostInitialized; 
}

void Transform::OnEnabled(){

}

void Transform::OnDisabled(){



}

void Transform::MarkDirty() {
    // Already dirty means every descendant is too: a clean child is only reachable
    // through GetWorldMatrix(), which recurses upward and cleans its parent first,
    // so "parent dirty, child clean" is unreachable. That makes this short-circuit
    // safe, and it collapses the 2nd and 3rd SetWorld* of a gizmo frame from a full
    // subtree walk to O(1).
    if (dirty) return;
    dirty = true;

    // The cached member, NOT Engine::Get()->GetActiveContainer(): during play mode an
    // editor-container Transform would otherwise resolve the *runtime* registry, find
    // none of its children there, and silently skip the whole subtree.
    if (!registry) return;   // setters can run before Init() caches it

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
    if (pos == localPosition) return;
    localPosition = pos;
    MarkDirty();
    Notify(POSITION_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void Transform::SetRotation(float degrees) {
    if (degrees == localRotation) return;
    localRotation = degrees;
    MarkDirty();
    Notify(ROTATION_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void Transform::SetScale(const glm::vec2& scale) {
    if (scale == localScale) return;
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
    // Compute scale magnitude
    float scaleX = glm::length(glm::vec2(world[0]));
    float scaleY = glm::length(glm::vec2(world[1]));
    
    // Determine sign by checking determinant of upper-left 2x2
    // Negative determinant means one axis is flipped (negative scale)
    float det = world[0][0] * world[1][1] - world[0][1] * world[1][0];
    if (det < 0) {
        // Convention: apply negative to X when flipped
        scaleX = -scaleX;
    }
    return glm::vec2(scaleX, scaleY);
}

void Transform::SetWorldPosition(const glm::vec2& pos) {
    if (pos == GetWorldPosition()) return;
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
    if (degrees == GetWorldRotation()) return;
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
    if (scale == GetWorldScale()) return;
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

    // Reparenting shuffles two transforms' child lists / this transform's
    // parent without touching the registry map, so invalidate every pointer
    // cache explicitly.
    Registry::BumpGeneration();

    MarkDirty();
    Notify(PARENT_CHANGED_EVENT, this->parent_id);
    Notify(CHANGED_EVENT);
}


void Transform::MoveChild(const std::string& childId, int targetIndex) {
    auto it = std::find(children_ids.begin(), children_ids.end(), childId);
    if (it == children_ids.end()) return;
    int oldIndex = (int)std::distance(children_ids.begin(), it);
    children_ids.erase(it);
    if (oldIndex < targetIndex) targetIndex--;
    targetIndex = std::max(0, std::min(targetIndex, (int)children_ids.size()));
    children_ids.insert(children_ids.begin() + targetIndex, childId);
    Registry::BumpGeneration(); // child order changed — refresh cached children
}

const std::vector<Transform*>& Transform::GetChildren() {
    const std::uint64_t gen = Registry::Generation();
    if (cachedChildrenGen == gen) return cachedChildren;

    cachedChildren.clear();
    if (!registry) {
        // Not yet initialized — leave the cache empty and unstamped so it
        // re-resolves once Init() wires up the registry. (No Alert: this is a
        // normal transient state during load, and it must not spam per frame.)
        return cachedChildren;
    }

    cachedChildren.reserve(children_ids.size());
    for (const std::string& id : children_ids) {
        if (Transform* child = registry->Find<Transform>(id))
            cachedChildren.push_back(child);
    }
    cachedChildrenGen = gen;
    return cachedChildren;
}


Transform* Transform::GetParent() {
    if (parent_id.empty())
        return nullptr;

    const std::uint64_t gen = Registry::Generation();
    if (cachedParentGen == gen && cachedParent) return cachedParent;

    if (!registry) {
        Console::Alert("Transform::GetParent called but registry is null (not initialized?)");
        return nullptr;
    }

    cachedParent = registry->Find<Transform>(parent_id);
    if (!cachedParent) {
        Console::Alert("Transform parent_id exists but is not a Transform");
        return nullptr;
    }
    cachedParentGen = gen;
    return cachedParent;
}




YAML::Node Transform::Serialize() {
    YAML::Node node = Component::Serialize();

    node["localPosition"][0] = localPosition.x;
    node["localPosition"][1] = localPosition.y;
    node["localPosition"].SetStyle(YAML::EmitterStyle::Flow);
    node["localRotation"] = localRotation;
    node["localScale"][0] = localScale.x;
    node["localScale"][1] = localScale.y;
    node["localScale"].SetStyle(YAML::EmitterStyle::Flow);
    if (parent_id.empty())
        node["parent_id"] = YAML::Node(YAML::NodeType::Null);
    else
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


void Transform::Accept(IVisitor* v) {
    
    v->Visit(this); 
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
    copy->state = State::Loaded;
    return copy;
}
