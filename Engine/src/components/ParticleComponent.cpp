#include "engine/components/ParticleComponent.hpp"
#include <algorithm>

void ParticleComponent::SetMaxParticles(int v)
{
    // Clamp to a sane range: 0 would allocate nothing; the upper bound guards
    // against an accidental huge SSBO from a fat-fingered inspector edit.
    v = std::clamp(v, 1, 1000000);
    if (v == maxParticles) return;
    maxParticles = v;
    Notify(MAX_PARTICLES_CHANGED_EVENT);
}

YAML::Node ParticleComponent::Serialize()
{
    YAML::Node node = Component::Serialize();

    node["emissionRate"] = emissionRate;
    node["maxParticles"] = maxParticles;
    node["duration"]     = duration;
    node["looping"]      = looping;
    node["startBurst"]   = startBurst;

    node["lifetimeMin"]  = lifetimeMin;
    node["lifetimeMax"]  = lifetimeMax;
    node["startSpeedMin"] = startSpeedMin;
    node["startSpeedMax"] = startSpeedMax;
    node["directionAngle"] = directionAngle;
    node["spreadDegrees"]  = spreadDegrees;

    node["gravity"][0] = gravity.x;
    node["gravity"][1] = gravity.y;
    node["gravity"].SetStyle(YAML::EmitterStyle::Flow);
    node["damping"] = damping;
    node["space"]   = static_cast<int>(space);

    node["shape"] = static_cast<int>(shape);
    node["shapeSize"][0] = shapeSize.x;
    node["shapeSize"][1] = shapeSize.y;
    node["shapeSize"].SetStyle(YAML::EmitterStyle::Flow);
    node["coneAngle"] = coneAngle;

    node["sprite_id"] = sprite_id;

    node["startColor"][0] = startColor.r;
    node["startColor"][1] = startColor.g;
    node["startColor"][2] = startColor.b;
    node["startColor"][3] = startColor.a;
    node["startColor"].SetStyle(YAML::EmitterStyle::Flow);

    node["endColor"][0] = endColor.r;
    node["endColor"][1] = endColor.g;
    node["endColor"][2] = endColor.b;
    node["endColor"][3] = endColor.a;
    node["endColor"].SetStyle(YAML::EmitterStyle::Flow);

    node["startSize"]   = startSize;
    node["endSize"]     = endSize;
    node["blendMode"]   = static_cast<int>(blendMode);
    node["sortingOrder"] = sortingOrder;

    return node;
}

void ParticleComponent::Deserialize(const YAML::Node& node)
{
    Component::Deserialize(node);

    emissionRate = node["emissionRate"].as<float>(20.0f);
    maxParticles = node["maxParticles"].as<int>(1000);
    duration     = node["duration"].as<float>(5.0f);
    looping      = node["looping"].as<bool>(true);
    startBurst   = node["startBurst"].as<int>(0);

    lifetimeMin  = node["lifetimeMin"].as<float>(1.0f);
    lifetimeMax  = node["lifetimeMax"].as<float>(2.0f);
    startSpeedMin = node["startSpeedMin"].as<float>(1.0f);
    startSpeedMax = node["startSpeedMax"].as<float>(2.0f);
    directionAngle = node["directionAngle"].as<float>(90.0f);
    spreadDegrees  = node["spreadDegrees"].as<float>(25.0f);

    if (node["gravity"])
        gravity = glm::vec2(node["gravity"][0].as<float>(), node["gravity"][1].as<float>());
    damping = node["damping"].as<float>(0.0f);
    space   = static_cast<SimulationSpace>(node["space"].as<int>(static_cast<int>(SimulationSpace::World)));

    shape = static_cast<Shape>(node["shape"].as<int>(static_cast<int>(Shape::Point)));
    if (node["shapeSize"])
        shapeSize = glm::vec2(node["shapeSize"][0].as<float>(), node["shapeSize"][1].as<float>());
    coneAngle = node["coneAngle"].as<float>(25.0f);

    sprite_id = node["sprite_id"].as<std::string>("");

    if (node["startColor"])
        startColor = glm::vec4(node["startColor"][0].as<float>(), node["startColor"][1].as<float>(),
                               node["startColor"][2].as<float>(), node["startColor"][3].as<float>());
    if (node["endColor"])
        endColor = glm::vec4(node["endColor"][0].as<float>(), node["endColor"][1].as<float>(),
                             node["endColor"][2].as<float>(), node["endColor"][3].as<float>());

    startSize    = node["startSize"].as<float>(0.3f);
    endSize      = node["endSize"].as<float>(0.0f);
    blendMode    = static_cast<BlendMode>(node["blendMode"].as<int>(static_cast<int>(BlendMode::Additive)));
    sortingOrder = node["sortingOrder"].as<int>(0);

    state = State::Loaded;
}

void ParticleComponent::Accept(IVisitor* v)
{
    v->Visit(this);
}

ParticleComponent* ParticleComponent::Copy()
{
    ParticleComponent* copy = new ParticleComponent();
    copy->id = id;
    copy->enabled = enabled;
    copy->gameobject_id = gameobject_id;

    copy->emissionRate = emissionRate;
    copy->maxParticles = maxParticles;
    copy->duration     = duration;
    copy->looping      = looping;
    copy->startBurst   = startBurst;

    copy->lifetimeMin   = lifetimeMin;
    copy->lifetimeMax   = lifetimeMax;
    copy->startSpeedMin = startSpeedMin;
    copy->startSpeedMax = startSpeedMax;
    copy->directionAngle = directionAngle;
    copy->spreadDegrees  = spreadDegrees;
    copy->gravity = gravity;
    copy->damping = damping;
    copy->space   = space;

    copy->shape     = shape;
    copy->shapeSize = shapeSize;
    copy->coneAngle = coneAngle;

    copy->sprite_id  = sprite_id;
    copy->startColor = startColor;
    copy->endColor   = endColor;
    copy->startSize  = startSize;
    copy->endSize    = endSize;
    copy->blendMode  = blendMode;
    copy->sortingOrder = sortingOrder;

    // pendingBurst intentionally not copied -- it's a transient request.
    return copy;
}
