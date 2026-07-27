#include "engine/components/Light.hpp"
#include "engine/components/Transform.hpp"
#include <algorithm>
#include <cmath>

namespace {
    constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
}

glm::vec2 Light::GetWorldDirection()
{
    float degrees = 0.0f;
    if (GameObject* obj = GetGameObject())
    {
        if (Transform* t = obj->GetTransform())
            degrees = t->GetWorldRotation();
    }
    const float rad = degrees * kDegToRad;
    return { std::cos(rad), std::sin(rad) };
}

void Light::SetType(LightType v)
{
    if (type == v) return;
    type = v;
    Notify(TYPE_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void Light::SetColor(const glm::vec4& v)
{
    color = v;
    Notify(COLOR_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void Light::SetIntensity(float v)
{
    if (intensity == v) return;
    intensity = std::max(0.0f, v);
    Notify(INTENSITY_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void Light::SetRange(float v)
{
    // A zero range would divide by zero in the shader's normalized distance.
    v = std::max(0.001f, v);
    if (range == v) return;
    range = v;
    Notify(RANGE_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void Light::SetInnerRadius(float v)
{
    // Clamped just below 1 so smoothstep(innerRadius, 1, d) never degenerates
    // to a zero-width edge (which reads as a hard ring rather than a falloff).
    v = std::clamp(v, 0.0f, 0.99f);
    if (innerRadius == v) return;
    innerRadius = v;
    Notify(INNER_RADIUS_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void Light::SetFalloff(float v)
{
    v = std::max(0.01f, v);
    if (falloff == v) return;
    falloff = v;
    Notify(FALLOFF_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void Light::SetInnerAngle(float v)
{
    v = std::clamp(v, 0.0f, 180.0f);
    if (innerAngle == v) return;
    innerAngle = v;
    // Inner must not exceed outer, or the cone's smoothstep inverts and the
    // spot renders inside-out. Push outer along rather than rejecting the edit.
    if (outerAngle < innerAngle)
    {
        outerAngle = innerAngle;
        Notify(OUTER_ANGLE_CHANGED_EVENT);
    }
    Notify(INNER_ANGLE_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void Light::SetOuterAngle(float v)
{
    v = std::clamp(v, 0.0f, 180.0f);
    if (outerAngle == v) return;
    outerAngle = v;
    if (innerAngle > outerAngle)
    {
        innerAngle = outerAngle;
        Notify(INNER_ANGLE_CHANGED_EVENT);
    }
    Notify(OUTER_ANGLE_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void Light::SetHeight(float v)
{
    if (height == v) return;
    height = v;
    Notify(HEIGHT_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void Light::SetNormalInfluence(float v)
{
    v = std::clamp(v, 0.0f, 1.0f);
    if (normalInfluence == v) return;
    normalInfluence = v;
    Notify(NORMAL_INFLUENCE_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void Light::SetCastShadows(bool v)
{
    if (castShadows == v) return;
    castShadows = v;
    Notify(CAST_SHADOWS_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void Light::SetShadowStrength(float v)
{
    v = std::clamp(v, 0.0f, 1.0f);
    if (shadowStrength == v) return;
    shadowStrength = v;
    Notify(SHADOW_STRENGTH_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

Light* Light::Copy()
{
    Light* copy = new Light();
    // id / enabled / gameobject_id are load-bearing: GameObject::component_ids
    // references this component by id, so a fresh id would orphan the copy.
    copy->id              = id;
    copy->enabled         = enabled;
    copy->gameobject_id   = gameobject_id;
    copy->type            = type;
    copy->color           = color;
    copy->intensity       = intensity;
    copy->range           = range;
    copy->innerRadius     = innerRadius;
    copy->falloff         = falloff;
    copy->innerAngle      = innerAngle;
    copy->outerAngle      = outerAngle;
    copy->height          = height;
    copy->normalInfluence = normalInfluence;
    copy->castShadows     = castShadows;
    copy->shadowStrength  = shadowStrength;
    return copy;
}

YAML::Node Light::Serialize()
{
    YAML::Node node = Component::Serialize();
    node["lightType"] = static_cast<int>(type);
    node["color"][0] = color.r;
    node["color"][1] = color.g;
    node["color"][2] = color.b;
    node["color"][3] = color.a;
    node["color"].SetStyle(YAML::EmitterStyle::Flow);
    node["intensity"]       = intensity;
    node["range"]           = range;
    node["innerRadius"]     = innerRadius;
    node["falloff"]         = falloff;
    node["innerAngle"]      = innerAngle;
    node["outerAngle"]      = outerAngle;
    node["height"]          = height;
    node["normalInfluence"] = normalInfluence;
    node["castShadows"]     = castShadows;
    node["shadowStrength"]  = shadowStrength;
    return node;
}

void Light::Deserialize(const YAML::Node& node)
{
    Component::Deserialize(node);
    // Every read is defaulted so a scene written before a field existed still loads.
    type = static_cast<LightType>(node["lightType"].as<int>(static_cast<int>(LightType::Point)));
    if (node["color"] && node["color"].IsSequence() && node["color"].size() == 4) {
        color = glm::vec4(
            node["color"][0].as<float>(1.0f),
            node["color"][1].as<float>(1.0f),
            node["color"][2].as<float>(1.0f),
            node["color"][3].as<float>(1.0f));
    }
    intensity       = node["intensity"].as<float>(1.0f);
    range           = node["range"].as<float>(300.0f);
    innerRadius     = node["innerRadius"].as<float>(0.0f);
    falloff         = node["falloff"].as<float>(1.0f);
    innerAngle      = node["innerAngle"].as<float>(25.0f);
    outerAngle      = node["outerAngle"].as<float>(45.0f);
    height          = node["height"].as<float>(50.0f);
    normalInfluence = node["normalInfluence"].as<float>(1.0f);
    castShadows     = node["castShadows"].as<bool>(false);
    shadowStrength  = node["shadowStrength"].as<float>(1.0f);
    state = State::Loaded;
}

void Light::Accept(IVisitor* v)
{
    v->Visit(this);
}
