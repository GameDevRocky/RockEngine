#pragma once
#include <vector>
#include "yaml-cpp/yaml.h"

// One control point on an AnimationCurve. Tangents are in value-units per
// time-unit (the same convention Unity uses), so a straight line between two keys
// has both tangents equal to the segment's slope.
struct Keyframe {
    float time       = 0.0f;
    float value      = 0.0f;
    float inTangent  = 0.0f;   // slope approaching this key from the left
    float outTangent = 0.0f;   // slope leaving this key to the right

    YAML::Node Serialize() const;
    void Deserialize(const YAML::Node& n);
};

// A keyframed float-over-float function, evaluated with cubic Hermite
// interpolation. Unity's AnimationCurve, minus the tangent modes.
//
// This is a plain VALUE TYPE -- deliberately not a Component, not a Serializable,
// and not registered with SerializableFactory. It has no identity and no
// lifecycle, so it copies by value: a component owning one satisfies the
// play-mode deep-copy invariant with a plain `copy->curve = curve;` rather than
// needing a Copy() of its own.
//
// It lives in utils/ rather than next to any one component because it is the
// natural authoring primitive for any "value over time" or "value over distance"
// field, not just TrailRenderer's width taper. It is also bound to Python as a
// real class (see AnimationCurveBindings.cpp) so scripts can build and evaluate
// curves directly.
class AnimationCurve {
public:
    // What Evaluate() does with a time outside the key range.
    //   Clamp    -- hold the first/last key's value (Unity's default)
    //   Loop     -- wrap back to the start, so the curve repeats
    //   PingPong -- reverse direction at each end
    enum class Wrap { Clamp, Loop, PingPong };

    AnimationCurve() = default;
    explicit AnimationCurve(std::vector<Keyframe> keys);

    // ── Factories ───────────────────────────────────────────────────────────
    static AnimationCurve Constant(float value);
    static AnimationCurve Linear(float t0, float v0, float t1, float v1);
    // Flat tangents at both ends -- an S-curve rather than a straight ramp.
    static AnimationCurve EaseInOut(float t0, float v0, float t1, float v1);

    // ── Evaluation ──────────────────────────────────────────────────────────
    // Never throws and never returns NaN. With no keys the result is 0; with one
    // key it is that key's value at every t.
    float Evaluate(float t) const;

    // ── Editing ─────────────────────────────────────────────────────────────
    // Keys are kept sorted by time at all times, so AddKey returns the index the
    // new key actually landed at rather than assuming it was appended.
    int  AddKey(float time, float value);
    int  AddKey(const Keyframe& key);
    void RemoveKey(int index);
    // Overwrite one key. Re-sorts (and returns the new index) if the edit moved it
    // past a neighbour in time -- dragging a handle in the curve editor does this
    // constantly.
    int  MoveKey(int index, const Keyframe& key);
    void Clear();

    const std::vector<Keyframe>& Keys() const { return keys; }
    int  KeyCount() const { return static_cast<int>(keys.size()); }

    Wrap GetWrap() const { return wrap; }
    void SetWrap(Wrap w) { wrap = w; }

    // Time span covered by the keys. Both are 0 when the curve is empty.
    float StartTime() const;
    float EndTime() const;

    // ── Serialization ───────────────────────────────────────────────────────
    // Emits { keys: [[time, value, inTangent, outTangent], ...], wrap: <int> }.
    // Deserialize defaults every field, so a missing or malformed node yields a
    // valid empty curve instead of throwing and taking down a whole scene load.
    YAML::Node Serialize() const;
    void Deserialize(const YAML::Node& node);

    bool operator==(const AnimationCurve& o) const;
    bool operator!=(const AnimationCurve& o) const { return !(*this == o); }

private:
    // Restore the sort-by-time invariant after an edit.
    void Sort();

    std::vector<Keyframe> keys;
    Wrap wrap = Wrap::Clamp;
};
