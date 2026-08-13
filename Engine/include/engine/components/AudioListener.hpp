#pragma once
#include "engine/components/Component.hpp"
#include "engine/serialization/SerializableFactory.hpp"
#include "yaml-cpp/yaml.h"

// Marks the GameObject whose Transform position drives the audio listener -- what every
// AudioSource's spatialBlend/distance attenuation measures against (see AudioEngine and
// AudioSource::UpdateSpatialPosition). No authored fields: pose is pulled from the Transform
// each frame, exactly like Camera. A scene needs one active AudioListener for positional audio
// to make sense, same as Unity; if none is present, Engine::Update falls back to Camera::GetMain()
// so spatial AudioSources still work out of the box (see GetMain()).
class AudioListener : public Component
{
public:
    // First enabled AudioListener found on an active GameObject, across every loaded scene of
    // the ACTIVE container. nullptr if none. Mirrors Camera::GetMain() minus the priority
    // tie-break (no authored fields here to prioritize on).
    static AudioListener* GetMain();

    AudioListener* Copy() override;
    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;
    void Accept(IVisitor* v) override;
    std::string GetTypeName() const override { return "AudioListener"; }
};
