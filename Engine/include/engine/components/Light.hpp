#pragma once
#include "engine/components/Component.hpp"
#include "engine/serialization/SerializableFactory.hpp"
#include "yaml-cpp/yaml.h"
#include <glm/glm.hpp>
#include <string>

// AUTHORED light settings. Pure data: no GL handles, no matrices, no cached
// GPU state -- the same rule Camera and ParticleComponent follow. LightingPass
// scans the scene each frame, resolves world pose off the Transform and packs
// these fields into the shared light UBO (see LightBuffer). That keeps Copy()
// a mechanical field copy so play mode's deep copy carries lights across for
// free, and means nothing here has to be torn down on shutdown.
//
// One component covers every light type via `type`, rather than three component
// classes: switching a light from Point to Spot is a routine authoring move and
// shouldn't cost a delete + re-add.
class Light : public Component
{
public:
    // Global is the ambient/fill term. It has no position, no direction and no
    // falloff -- LightingPass folds every Global light into the single ambient
    // value in the UBO. It exists because a lighting system without an ambient
    // term leaves unlit regions at pure black with no way to lift them.
    enum class LightType { Point = 0, Spot = 1, Directional = 2, Global = 3 };

    // One change event per inspector-editable field (drives the deferred
    // inspector refresh + undo coalescing, exactly like SpriteRenderer).
    static inline const Event CHANGED_EVENT                  = Light::CreateEvent();
    static inline const Event TYPE_CHANGED_EVENT             = Light::CreateEvent();
    static inline const Event COLOR_CHANGED_EVENT            = Light::CreateEvent();
    static inline const Event INTENSITY_CHANGED_EVENT        = Light::CreateEvent();
    static inline const Event RANGE_CHANGED_EVENT            = Light::CreateEvent();
    static inline const Event INNER_RADIUS_CHANGED_EVENT     = Light::CreateEvent();
    static inline const Event FALLOFF_CHANGED_EVENT          = Light::CreateEvent();
    static inline const Event INNER_ANGLE_CHANGED_EVENT      = Light::CreateEvent();
    static inline const Event OUTER_ANGLE_CHANGED_EVENT      = Light::CreateEvent();
    static inline const Event HEIGHT_CHANGED_EVENT           = Light::CreateEvent();
    static inline const Event NORMAL_INFLUENCE_CHANGED_EVENT = Light::CreateEvent();
    static inline const Event CAST_SHADOWS_CHANGED_EVENT     = Light::CreateEvent();
    static inline const Event SHADOW_STRENGTH_CHANGED_EVENT  = Light::CreateEvent();

    Light* Copy() override;
    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;
    void Accept(IVisitor* v) override;
    // MUST be const -- Component::GetTypeName() is `const = 0` while
    // Serializable::GetTypeName() is a separate non-const virtual. Dropping the
    // const here fails to satisfy the pure virtual and is a compile error.
    std::string GetTypeName() const override { return "Light"; }

    // ── Type ────────────────────────────────────────────────────────────────
    LightType GetType() const { return type; }
    void SetType(LightType v);

    // ── Common ──────────────────────────────────────────────────────────────
    const glm::vec4& GetColor() const { return color; }
    void SetColor(const glm::vec4& v);

    float GetIntensity() const { return intensity; }
    void  SetIntensity(float v);

    // ── Point / Spot attenuation ────────────────────────────────────────────
    // World units == pixels (see EngineUtils::RenderUtils), so a range of 300
    // reaches 300px at zoom 1.
    float GetRange() const { return range; }
    void  SetRange(float v);

    // Fraction of `range` (0..1) that stays at full brightness before falloff
    // begins. 0 = falloff starts at the light's centre.
    float GetInnerRadius() const { return innerRadius; }
    void  SetInnerRadius(float v);

    // Attenuation exponent: 1 = linear-ish ramp, higher = tighter hotspot.
    float GetFalloff() const { return falloff; }
    void  SetFalloff(float v);

    // ── Spot cone ───────────────────────────────────────────────────────────
    // Half-angles in degrees. Inside innerAngle the cone is at full strength;
    // between inner and outer it ramps to zero.
    float GetInnerAngle() const { return innerAngle; }
    void  SetInnerAngle(float v);

    float GetOuterAngle() const { return outerAngle; }
    void  SetOuterAngle(float v);

    // ── Normal-map shading ──────────────────────────────────────────────────
    // Height above the sprite plane. Sprites are all at z=0, so without a Z
    // offset every light vector would be perfectly in-plane and a normal map
    // would produce no shading at all. This is the light's "how far off the
    // page" distance.
    float GetHeight() const { return height; }
    void  SetHeight(float v);

    // 0 = ignore the surface normal entirely (flat lighting), 1 = full N·L.
    float GetNormalInfluence() const { return normalInfluence; }
    void  SetNormalInfluence(float v);

    // ── Shadows ─────────────────────────────────────────────────────────────
    bool GetCastShadows() const { return castShadows; }
    void SetCastShadows(bool v);

    float GetShadowStrength() const { return shadowStrength; }
    void  SetShadowStrength(float v);

    // Unit direction for Spot/Directional, derived from the owning Transform's
    // world rotation (degrees, CCW from +X). There is deliberately no authored
    // direction field: the transform rotate gizmo already edits this, and two
    // sources of truth would drift.
    glm::vec2 GetWorldDirection();

private:
    LightType type = LightType::Point;

    glm::vec4 color     = { 1.0f, 1.0f, 1.0f, 1.0f };
    float     intensity = 1.0f;

    float range       = 300.0f;
    float innerRadius = 0.0f;
    float falloff     = 1.0f;

    float innerAngle = 25.0f;
    float outerAngle = 45.0f;

    float height          = 50.0f;
    float normalInfluence = 1.0f;

    bool  castShadows    = false;
    float shadowStrength = 1.0f;
};
