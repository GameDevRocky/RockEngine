#pragma once
#include "engine/components/Component.hpp"
#include "engine/serialization/SerializableFactory.hpp"
#include "engine/rendering/TrailMesh.hpp"
#include "engine/utils/AnimationCurve.hpp"
#include "yaml-cpp/yaml.h"
#include <glm/glm.hpp>
#include <cstdint>
#include <deque>
#include <string>

class Material;
class Sprite;

// A ribbon that follows the object, Unity's TrailRenderer.
//
// The component holds authored config plus the recorded point history; it owns
// NO GL state. The vertex buffer lives in TrailManager keyed by this id, for the
// same reason FontManager holds glyph meshes: play mode deep-copies the editor
// world and a GL handle on a component would be duplicated by Copy() and then
// double-deleted. The point history is plain CPU memory, so it is safe here --
// it simply must not survive Copy() (see the note there).
//
// Points are recorded in LateUpdate and drawn by ScenePass, which sorts trails
// against sprites, text and particles in one list by (layer, order).
class TrailRenderer : public Component
{
public:
    // Asset NAME (not id) of the material a fresh TrailRenderer starts with, so
    // it draws something immediately. A name because ids live in meta files and
    // are regenerated on collision -- same reasoning as TextRenderer.
    static constexpr const char* kDefaultMaterialName = "trail";

    TrailRenderer();

    // One change event per inspector-editable field: the Inspector's deferred
    // refresh and the undo bridge both key off these.
    static inline const Event TIME_CHANGED_EVENT                = TrailRenderer::CreateEvent();
    static inline const Event MIN_VERTEX_DISTANCE_CHANGED_EVENT = TrailRenderer::CreateEvent();
    static inline const Event WIDTH_CURVE_CHANGED_EVENT         = TrailRenderer::CreateEvent();
    static inline const Event WIDTH_MULTIPLIER_CHANGED_EVENT    = TrailRenderer::CreateEvent();
    static inline const Event START_COLOR_CHANGED_EVENT         = TrailRenderer::CreateEvent();
    static inline const Event END_COLOR_CHANGED_EVENT           = TrailRenderer::CreateEvent();
    static inline const Event EMITTING_CHANGED_EVENT            = TrailRenderer::CreateEvent();
    static inline const Event AUTODESTRUCT_CHANGED_EVENT        = TrailRenderer::CreateEvent();
    static inline const Event MATERIAL_CHANGED_EVENT            = TrailRenderer::CreateEvent();
    static inline const Event SPRITE_CHANGED_EVENT              = TrailRenderer::CreateEvent();
    static inline const Event SORTING_LAYER_CHANGED_EVENT       = TrailRenderer::CreateEvent();
    static inline const Event SORTING_ORDER_CHANGED_EVENT       = TrailRenderer::CreateEvent();

    TrailRenderer* Copy() override;
    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;
    void Accept(IVisitor* v) override;
    std::string GetTypeName() const override { return "TrailRenderer"; }

    // Records the head and expires the tail. Deliberately NOT gated on
    // Container::Mode::Runtime (unlike ScriptComponent and Animator): a trail
    // that only exists in play mode cannot be tuned, so it previews live while
    // the object is dragged around the Scene view.
    void LateUpdate() override;

    // ── Shape ───────────────────────────────────────────────────────────────
    float GetTime() const { return time; }
    void  SetTime(float v);

    float GetMinVertexDistance() const { return minVertexDistance; }
    void  SetMinVertexDistance(float v);

    const AnimationCurve& GetWidthCurve() const { return widthCurve; }
    void SetWidthCurve(const AnimationCurve& c) { widthCurve = c; Notify(WIDTH_CURVE_CHANGED_EVENT); }

    float GetWidthMultiplier() const { return widthMultiplier; }
    void  SetWidthMultiplier(float v) { widthMultiplier = v; Notify(WIDTH_MULTIPLIER_CHANGED_EVENT); }

    // ── Appearance ──────────────────────────────────────────────────────────
    glm::vec4 GetStartColor() const { return startColor; }
    void SetStartColor(const glm::vec4& v) { startColor = v; Notify(START_COLOR_CHANGED_EVENT); }

    glm::vec4 GetEndColor() const { return endColor; }
    void SetEndColor(const glm::vec4& v) { endColor = v; Notify(END_COLOR_CHANGED_EVENT); }

    Material* GetMaterial();
    const std::string& GetMaterialID() const { return material_id; }
    void SetMaterial(const std::string& id) { material_id = id; Notify(MATERIAL_CHANGED_EVENT); }

    Sprite* GetSprite();
    const std::string& GetSpriteID() const { return sprite_id; }
    void SetSprite(const std::string& id) { sprite_id = id; Notify(SPRITE_CHANGED_EVENT); }

    // ── Emission ────────────────────────────────────────────────────────────
    // Stops laying down new points; the existing tail still ages out, which is
    // what makes a trail trail off rather than vanish.
    bool GetEmitting() const { return emitting; }
    void SetEmitting(bool v) { emitting = v; Notify(EMITTING_CHANGED_EVENT); }

    // Destroy the owning GameObject once emitting has stopped and the last point
    // has expired. Runtime only -- an edit-mode preview must never delete
    // authored objects.
    bool GetAutodestruct() const { return autodestruct; }
    void SetAutodestruct(bool v) { autodestruct = v; Notify(AUTODESTRUCT_CHANGED_EVENT); }

    // ── Sorting (same model as SpriteRenderer) ──────────────────────────────
    const std::string& GetSortingLayer() const { return sortingLayer; }
    void SetSortingLayer(const std::string& v) { if (sortingLayer == v) return; sortingLayer = v; Notify(SORTING_LAYER_CHANGED_EVENT); }

    int  GetSortingOrder() const { return sortingOrder; }
    void SetSortingOrder(int v) { sortingOrder = v; Notify(SORTING_ORDER_CHANGED_EVENT); }

    // ── Transient history (not serialized, not copied) ──────────────────────
    // front() is the head (newest), back() the tail (oldest) -- the order
    // TrailMesh::Build expects, and both ends are O(1) on a deque.
    const std::deque<TrailPoint>& GetPoints() const { return points; }

    // Bumped on every record/expire. TrailManager uses it to decide whether the
    // GPU buffer is stale, in place of the content hash FontManager uses: a
    // trail's mesh changes almost every frame, so hashing its state would cost
    // about as much as rebuilding it.
    std::uint64_t GetRevision() const { return revision; }

    // Drop the whole history immediately (teleporting an object without dragging
    // a ribbon across the screen behind it). Exposed as an Inspector button and
    // to MCP via ComponentActions.
    void Clear();

    // Push the trail's authored appearance into the bound shader, continuing
    // texture-slot allocation from where the material stopped. Mirrors
    // SpriteRenderer::OverrideUniforms.
    void OverrideUniforms(int firstFreeTextureSlot);

private:
    // Shape
    float time = 1.0f;                 // seconds a point survives
    float minVertexDistance = 2.0f;    // world units (== pixels)
    AnimationCurve widthCurve;         // seeded in the constructor: 1 at head -> 0 at tail
    float widthMultiplier = 16.0f;     // pixels

    // Appearance
    glm::vec4 startColor{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 endColor{1.0f, 1.0f, 1.0f, 0.0f};
    std::string material_id = "";
    std::string sprite_id = "";

    // Emission
    bool emitting = true;
    bool autodestruct = false;

    std::string sortingLayer = "Default";
    int sortingOrder = 0;

    // Transient
    std::deque<TrailPoint> points;
    std::uint64_t revision = 0;
};
