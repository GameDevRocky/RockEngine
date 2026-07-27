#pragma once
#include "engine/components/Component.hpp"
#include "engine/serialization/SerializableFactory.hpp"
#include "yaml-cpp/yaml.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>

// Marks a GameObject as an occluder for shadow-casting Lights.
//
// Like Light, this is authored config only -- no GL, no cached geometry. Each
// frame LightingPass calls AppendSegments() on every enabled caster to build
// one flat world-space edge list, then rasterizes that list into the shadow
// atlas once per shadow-casting light. Rebuilding the segments per frame (rather
// than caching and invalidating) keeps casters correct through transform
// changes, parenting, animation and physics with no dirty-tracking at all --
// the same "pull at render time" rule the cameras follow.
class ShadowCaster : public Component
{
public:
    // SpriteAlpha is the default so dropping this on a sprite Just Works AND
    // looks right: light passes through the sprite's transparent pixels instead
    // of being blocked by its bounding box.
    //
    // SpriteBounds (the plain rect) is kept because it is far cheaper -- a solid
    // tile or wall needs 4 segments, not ~40 -- and for fully-opaque art the two
    // are identical.
    //
    // FromCollider exists so the physics shape and the shadow silhouette can't
    // silently drift apart on objects that already have a collider.
    enum class Shape { SpriteBounds = 0, Box = 1, Circle = 2, FromCollider = 3, SpriteAlpha = 4 };

    static inline const Event CHANGED_EVENT         = ShadowCaster::CreateEvent();
    static inline const Event SHAPE_CHANGED_EVENT   = ShadowCaster::CreateEvent();
    static inline const Event CENTER_CHANGED_EVENT  = ShadowCaster::CreateEvent();
    static inline const Event SIZE_CHANGED_EVENT    = ShadowCaster::CreateEvent();
    static inline const Event RADIUS_CHANGED_EVENT  = ShadowCaster::CreateEvent();
    static inline const Event SEGMENTS_CHANGED_EVENT = ShadowCaster::CreateEvent();
    static inline const Event ALPHA_THRESHOLD_CHANGED_EVENT = ShadowCaster::CreateEvent();

    ShadowCaster* Copy() override;
    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;
    void Accept(IVisitor* v) override;
    std::string GetTypeName() const override { return "ShadowCaster"; }

    Shape GetShape() const { return shape; }
    void  SetShape(Shape v);

    // Local-space offset from the Transform origin, in pixels.
    const glm::vec2& GetCenter() const { return center; }
    void SetCenter(const glm::vec2& v);

    // Box extents in pixels (full width/height, not half).
    const glm::vec2& GetSize() const { return size; }
    void SetSize(const glm::vec2& v);

    float GetRadius() const { return radius; }
    void  SetRadius(float v);

    // How many edges approximate a Circle silhouette. Higher = rounder shadow
    // boundary at the cost of more occluder geometry per light.
    int  GetCircleSegments() const { return circleSegments; }
    void SetCircleSegments(int v);

    // SpriteAlpha only: pixels at or above this alpha block light. Raise it to
    // stop soft anti-aliased edges from casting, lower it to include them.
    float GetAlphaThreshold() const { return alphaThreshold; }
    void  SetAlphaThreshold(float v);

    // Appends this caster's world-space silhouette as flat point PAIRS:
    // {a0, b0, a1, b1, ...}. Appends nothing when the shape can't be resolved
    // (e.g. FromCollider on an object with no collider).
    void AppendSegments(std::vector<glm::vec2>& out);

    // The silhouette as a closed world-space polyline (one point per corner, no
    // duplicated endpoints) for the shapes that HAVE one. SpriteAlpha has no
    // single closed loop -- a sprite can have holes and disjoint islands -- so it
    // returns nothing here and the gizmo falls back to drawing its segments.
    void BuildOutline(std::vector<glm::vec2>& out);

private:
    // Resolves the sprite quad's local-space centre, matching the vertex shader's
    // `aPos * uSize - uSize * uPivot` exactly. Returns false if there is no
    // sprite to measure. Shared by the SpriteBounds and SpriteAlpha paths so the
    // two can't disagree about where the sprite actually is.
    bool ResolveSpriteQuad(class Sprite*& outSprite, glm::vec2& outLocalCenter,
                           glm::vec2& outSize);

    Shape     shape          = Shape::SpriteAlpha;
    glm::vec2 center         = { 0.0f, 0.0f };
    glm::vec2 size           = { 100.0f, 100.0f };
    float     radius         = 50.0f;
    int       circleSegments = 16;
    float     alphaThreshold = 0.5f;
};
