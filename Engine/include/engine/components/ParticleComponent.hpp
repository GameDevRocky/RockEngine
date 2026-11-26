#pragma once
#include "engine/components/Component.hpp"
#include "engine/serialization/SerializableFactory.hpp"
#include "yaml-cpp/yaml.h"
#include <glm/glm.hpp>
#include <string>

// A GPU-simulated particle emitter. This component holds ONLY authored config +
// identity -- no GL handles and no live particle array. All simulation happens
// on the GPU inside ParticleSimulationPass/ParticleManager, keyed by this id
// (ScenePass does the drawing).
// That keeps Copy() trivial (config only) and lets play mode deep-copy an
// emitter like any other component. See Engine/CLAUDE.md "Rendering" and the
// particle plan for why simulation lives in the render pass, not here.
class ParticleComponent : public Component
{
public:
    enum class Shape { Point, Circle, Box, Cone };
    enum class SimulationSpace { Local, World };
    enum class BlendMode { Alpha, Additive };

    // One change event per inspector-editable field (drives the deferred
    // inspector refresh + undo, exactly like SpriteRenderer).
    static inline const Event EMISSION_RATE_CHANGED_EVENT = ParticleComponent::CreateEvent();
    static inline const Event MAX_PARTICLES_CHANGED_EVENT = ParticleComponent::CreateEvent();
    static inline const Event DURATION_CHANGED_EVENT       = ParticleComponent::CreateEvent();
    static inline const Event LOOPING_CHANGED_EVENT        = ParticleComponent::CreateEvent();
    static inline const Event START_BURST_CHANGED_EVENT    = ParticleComponent::CreateEvent();
    static inline const Event LIFETIME_CHANGED_EVENT       = ParticleComponent::CreateEvent();
    static inline const Event SPEED_CHANGED_EVENT          = ParticleComponent::CreateEvent();
    static inline const Event DIRECTION_CHANGED_EVENT      = ParticleComponent::CreateEvent();
    static inline const Event SPREAD_CHANGED_EVENT         = ParticleComponent::CreateEvent();
    static inline const Event GRAVITY_CHANGED_EVENT        = ParticleComponent::CreateEvent();
    static inline const Event DAMPING_CHANGED_EVENT        = ParticleComponent::CreateEvent();
    static inline const Event SPACE_CHANGED_EVENT          = ParticleComponent::CreateEvent();
    static inline const Event SHAPE_CHANGED_EVENT          = ParticleComponent::CreateEvent();
    static inline const Event SHAPE_SIZE_CHANGED_EVENT     = ParticleComponent::CreateEvent();
    static inline const Event CONE_ANGLE_CHANGED_EVENT     = ParticleComponent::CreateEvent();
    static inline const Event SPRITE_CHANGED_EVENT         = ParticleComponent::CreateEvent();
    static inline const Event FLIP_X_CHANGED_EVENT         = ParticleComponent::CreateEvent();
    static inline const Event FLIP_Y_CHANGED_EVENT         = ParticleComponent::CreateEvent();
    static inline const Event START_COLOR_CHANGED_EVENT    = ParticleComponent::CreateEvent();
    static inline const Event END_COLOR_CHANGED_EVENT      = ParticleComponent::CreateEvent();
    static inline const Event START_SIZE_CHANGED_EVENT     = ParticleComponent::CreateEvent();
    static inline const Event END_SIZE_CHANGED_EVENT       = ParticleComponent::CreateEvent();
    static inline const Event BLEND_MODE_CHANGED_EVENT     = ParticleComponent::CreateEvent();
    static inline const Event SORTING_LAYER_CHANGED_EVENT  = ParticleComponent::CreateEvent();
    static inline const Event SORTING_ORDER_CHANGED_EVENT  = ParticleComponent::CreateEvent();

    ParticleComponent* Copy() override;
    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;
    void Accept(IVisitor* v) override;
    std::string GetTypeName() const override { return "ParticleComponent"; }

    // ── Emission ────────────────────────────────────────────────────────────
    float GetEmissionRate() const { return emissionRate; }
    void  SetEmissionRate(float v) { emissionRate = v; Notify(EMISSION_RATE_CHANGED_EVENT); }

    int  GetMaxParticles() const { return maxParticles; }
    void SetMaxParticles(int v);

    float GetDuration() const { return duration; }
    void  SetDuration(float v) { duration = v; Notify(DURATION_CHANGED_EVENT); }

    bool GetLooping() const { return looping; }
    void SetLooping(bool v) { looping = v; Notify(LOOPING_CHANGED_EVENT); }

    int  GetStartBurst() const { return startBurst; }
    void SetStartBurst(int v) { startBurst = v; Notify(START_BURST_CHANGED_EVENT); }

    // ── Lifetime / motion ───────────────────────────────────────────────────
    glm::vec2 GetLifetime() const { return { lifetimeMin, lifetimeMax }; }   // x=min, y=max
    void SetLifetime(const glm::vec2& v) { lifetimeMin = v.x; lifetimeMax = v.y; Notify(LIFETIME_CHANGED_EVENT); }

    glm::vec2 GetSpeed() const { return { startSpeedMin, startSpeedMax }; }  // x=min, y=max
    void SetSpeed(const glm::vec2& v) { startSpeedMin = v.x; startSpeedMax = v.y; Notify(SPEED_CHANGED_EVENT); }

    float GetDirection() const { return directionAngle; }
    void  SetDirection(float v) { directionAngle = v; Notify(DIRECTION_CHANGED_EVENT); }

    float GetSpread() const { return spreadDegrees; }
    void  SetSpread(float v) { spreadDegrees = v; Notify(SPREAD_CHANGED_EVENT); }

    glm::vec2 GetGravity() const { return gravity; }
    void  SetGravity(const glm::vec2& v) { gravity = v; Notify(GRAVITY_CHANGED_EVENT); }

    float GetDamping() const { return damping; }
    void  SetDamping(float v) { damping = v; Notify(DAMPING_CHANGED_EVENT); }

    SimulationSpace GetSpace() const { return space; }
    void SetSpace(SimulationSpace v) { space = v; Notify(SPACE_CHANGED_EVENT); }

    // ── Shape ───────────────────────────────────────────────────────────────
    Shape GetShape() const { return shape; }
    void  SetShape(Shape v) { shape = v; Notify(SHAPE_CHANGED_EVENT); }

    glm::vec2 GetShapeSize() const { return shapeSize; }
    void  SetShapeSize(const glm::vec2& v) { shapeSize = v; Notify(SHAPE_SIZE_CHANGED_EVENT); }

    float GetConeAngle() const { return coneAngle; }
    void  SetConeAngle(float v) { coneAngle = v; Notify(CONE_ANGLE_CHANGED_EVENT); }

    // ── Appearance ──────────────────────────────────────────────────────────
    const std::string& GetSpriteID() const { return sprite_id; }
    void SetSprite(const std::string& id) { sprite_id = id; Notify(SPRITE_CHANGED_EVENT); }

    // Mirror the sprite on every particle. Applied as a negated UV scale about the
    // sprite's own sub-rect (see ScenePass), so flipping an atlas sprite stays
    // inside its own region instead of sampling a neighbour.
    bool GetFlipX() const { return flipX; }
    void SetFlipX(bool v) { flipX = v; Notify(FLIP_X_CHANGED_EVENT); }

    bool GetFlipY() const { return flipY; }
    void SetFlipY(bool v) { flipY = v; Notify(FLIP_Y_CHANGED_EVENT); }

    glm::vec4 GetStartColor() const { return startColor; }
    void  SetStartColor(const glm::vec4& v) { startColor = v; Notify(START_COLOR_CHANGED_EVENT); }

    glm::vec4 GetEndColor() const { return endColor; }
    void  SetEndColor(const glm::vec4& v) { endColor = v; Notify(END_COLOR_CHANGED_EVENT); }

    float GetStartSize() const { return startSize; }
    void  SetStartSize(float v) { startSize = v; Notify(START_SIZE_CHANGED_EVENT); }

    float GetEndSize() const { return endSize; }
    void  SetEndSize(float v) { endSize = v; Notify(END_SIZE_CHANGED_EVENT); }

    BlendMode GetBlendMode() const { return blendMode; }
    void SetBlendMode(BlendMode v) { blendMode = v; Notify(BLEND_MODE_CHANGED_EVENT); }

    // Same sorting model as SpriteRenderer: emitters are ordered against SPRITES
    // by (layer priority, order in layer) in one combined list, so a particle
    // system can sit behind or between sprite layers instead of always on top.
    // See ScenePass, which draws both.
    const std::string& GetSortingLayer() const { return sortingLayer; }
    void SetSortingLayer(const std::string& v) { if (sortingLayer == v) return; sortingLayer = v; Notify(SORTING_LAYER_CHANGED_EVENT); }

    int  GetSortingOrder() const { return sortingOrder; }
    void SetSortingOrder(int v) { sortingOrder = v; Notify(SORTING_ORDER_CHANGED_EVENT); }

    // ── Transient runtime request (not serialized, not copied) ──────────────
    // Manual/one-shot burst. The inspector's "Emit Burst" button and scripts
    // call this; ParticleManager drains it once per simulated frame. Kept off
    // Copy()/Serialize() deliberately -- it is an ephemeral request, not state.
    void EmitBurst(int count) { if (count > 0) pendingBurst += count; }
    int  ConsumePendingBurst() { int n = pendingBurst; pendingBurst = 0; return n; }

private:
    // Emission
    float emissionRate = 20.0f;
    int   maxParticles = 1000;
    float duration     = 5.0f;
    bool  looping      = true;
    int   startBurst   = 0;

    // Lifetime / motion
    float lifetimeMin = 1.0f;
    float lifetimeMax = 2.0f;
    float startSpeedMin = 1.0f;
    float startSpeedMax = 2.0f;
    float directionAngle = 90.0f;   // degrees, CCW from +X (90 = up)
    float spreadDegrees  = 25.0f;
    glm::vec2 gravity = { 0.0f, -2.0f };
    float damping = 0.0f;           // per-second velocity drag; vel *= exp(-damping*dt)
    SimulationSpace space = SimulationSpace::World;

    // Shape (shapeSize in pixels, like sprite pixel sizes -- see PixelsToWorld)
    Shape shape = Shape::Point;
    glm::vec2 shapeSize = { 32.0f, 32.0f };
    float coneAngle = 25.0f;

    // Appearance (sizes in pixels)
    std::string sprite_id = "";
    bool flipX = false;
    bool flipY = false;
    glm::vec4 startColor = { 1.0f, 0.9f, 0.4f, 1.0f };
    glm::vec4 endColor   = { 1.0f, 0.2f, 0.0f, 0.0f };
    float startSize = 24.0f;
    float endSize   = 0.0f;
    BlendMode blendMode = BlendMode::Additive;
    std::string sortingLayer = "Default";
    int   sortingOrder  = 0;

    int pendingBurst = 0;  // transient
};
