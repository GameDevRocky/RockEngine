#include "engine/utils/AnimationCurve.hpp"

#include <algorithm>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
// Keyframe
// ─────────────────────────────────────────────────────────────────────────────
YAML::Node Keyframe::Serialize() const {
    // A flow sequence rather than a map: a curve is a list of these, and four
    // named keys per entry turns a two-key taper into fourteen lines of scene
    // file. [time, value, inTangent, outTangent] stays on one line and is still
    // readable by a human diffing a .scene.
    YAML::Node n;
    n.push_back(time);
    n.push_back(value);
    n.push_back(inTangent);
    n.push_back(outTangent);
    n.SetStyle(YAML::EmitterStyle::Flow);
    return n;
}

void Keyframe::Deserialize(const YAML::Node& n) {
    if (!n || !n.IsSequence()) return;
    if (n.size() > 0) time       = n[0].as<float>(0.0f);
    if (n.size() > 1) value      = n[1].as<float>(0.0f);
    if (n.size() > 2) inTangent  = n[2].as<float>(0.0f);
    if (n.size() > 3) outTangent = n[3].as<float>(0.0f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────
AnimationCurve::AnimationCurve(std::vector<Keyframe> k) : keys(std::move(k)) {
    Sort();
}

AnimationCurve AnimationCurve::Constant(float value) {
    return AnimationCurve({ Keyframe{ 0.0f, value, 0.0f, 0.0f } });
}

AnimationCurve AnimationCurve::Linear(float t0, float v0, float t1, float v1) {
    // Both tangents equal the segment slope, which makes the Hermite basis
    // collapse to a straight line -- so Linear() really is linear rather than
    // merely looking close.
    const float dt = t1 - t0;
    const float slope = (std::abs(dt) > 1e-6f) ? (v1 - v0) / dt : 0.0f;
    return AnimationCurve({
        Keyframe{ t0, v0, slope, slope },
        Keyframe{ t1, v1, slope, slope }
    });
}

AnimationCurve AnimationCurve::EaseInOut(float t0, float v0, float t1, float v1) {
    // Flat at both ends: the classic smoothstep shape.
    return AnimationCurve({
        Keyframe{ t0, v0, 0.0f, 0.0f },
        Keyframe{ t1, v1, 0.0f, 0.0f }
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// Editing
// ─────────────────────────────────────────────────────────────────────────────
void AnimationCurve::Sort() {
    std::stable_sort(keys.begin(), keys.end(),
                     [](const Keyframe& a, const Keyframe& b) { return a.time < b.time; });
}

int AnimationCurve::AddKey(float time, float value) {
    return AddKey(Keyframe{ time, value, 0.0f, 0.0f });
}

int AnimationCurve::AddKey(const Keyframe& key) {
    // Insert in place rather than push-then-sort so the returned index is the
    // real resting position -- the curve editor uses it to keep the newly added
    // handle selected.
    auto it = std::upper_bound(keys.begin(), keys.end(), key.time,
                               [](float t, const Keyframe& k) { return t < k.time; });
    const int index = static_cast<int>(it - keys.begin());
    keys.insert(it, key);
    return index;
}

void AnimationCurve::RemoveKey(int index) {
    if (index < 0 || index >= static_cast<int>(keys.size())) return;
    keys.erase(keys.begin() + index);
}

int AnimationCurve::MoveKey(int index, const Keyframe& key) {
    if (index < 0 || index >= static_cast<int>(keys.size())) return index;
    keys[index] = key;

    // Only re-sort when the edit actually crossed a neighbour. Sorting on every
    // frame of a drag would be harmless but would also renumber indices the
    // caller is still holding.
    const bool beforePrev = index > 0 && key.time < keys[index - 1].time;
    const bool afterNext  = index + 1 < static_cast<int>(keys.size())
                            && key.time > keys[index + 1].time;
    if (!beforePrev && !afterNext) return index;

    Sort();
    // Identity is gone after a sort, so match on the exact pair just written.
    for (int i = 0; i < static_cast<int>(keys.size()); ++i) {
        if (keys[i].time == key.time && keys[i].value == key.value) return i;
    }
    return index;
}

void AnimationCurve::Clear() { keys.clear(); }

float AnimationCurve::StartTime() const { return keys.empty() ? 0.0f : keys.front().time; }
float AnimationCurve::EndTime()   const { return keys.empty() ? 0.0f : keys.back().time; }

// ─────────────────────────────────────────────────────────────────────────────
// Evaluation
// ─────────────────────────────────────────────────────────────────────────────
float AnimationCurve::Evaluate(float t) const {
    if (keys.empty()) return 0.0f;
    if (keys.size() == 1) return keys.front().value;

    const float t0 = keys.front().time;
    const float t1 = keys.back().time;
    const float span = t1 - t0;

    // A zero-width span would divide by zero in every wrap mode below, and there
    // is no meaningful interpolation across it anyway.
    if (span <= 1e-6f) return keys.front().value;

    if (t <= t0 || t >= t1) {
        switch (wrap) {
            case Wrap::Loop: {
                float u = std::fmod(t - t0, span);
                if (u < 0.0f) u += span;
                t = t0 + u;
                break;
            }
            case Wrap::PingPong: {
                float u = std::fmod(t - t0, 2.0f * span);
                if (u < 0.0f) u += 2.0f * span;
                if (u > span) u = 2.0f * span - u;
                t = t0 + u;
                break;
            }
            case Wrap::Clamp:
            default:
                return (t <= t0) ? keys.front().value : keys.back().value;
        }
    }

    // Bracketing pair. upper_bound gives the first key strictly after t, so the
    // segment is [it-1, it]; both are valid because t is now inside the range and
    // there are at least two keys.
    auto it = std::upper_bound(keys.begin(), keys.end(), t,
                               [](float time, const Keyframe& k) { return time < k.time; });
    if (it == keys.begin()) return keys.front().value;
    if (it == keys.end())   return keys.back().value;

    const Keyframe& a = *(it - 1);
    const Keyframe& b = *it;

    const float dt = b.time - a.time;
    if (dt <= 1e-6f) return b.value;   // coincident keys: take the later one

    const float u  = (t - a.time) / dt;
    const float u2 = u * u;
    const float u3 = u2 * u;

    // Cubic Hermite basis. The tangents are scaled by dt because they are
    // expressed in value-per-time while the basis works in the normalized [0,1]
    // segment parameter.
    const float h00 =  2.0f * u3 - 3.0f * u2 + 1.0f;
    const float h10 =         u3 - 2.0f * u2 + u;
    const float h01 = -2.0f * u3 + 3.0f * u2;
    const float h11 =         u3 -        u2;

    return h00 * a.value
         + h10 * dt * a.outTangent
         + h01 * b.value
         + h11 * dt * b.inTangent;
}

// ─────────────────────────────────────────────────────────────────────────────
// Serialization
// ─────────────────────────────────────────────────────────────────────────────
YAML::Node AnimationCurve::Serialize() const {
    YAML::Node node;
    YAML::Node keyList(YAML::NodeType::Sequence);
    for (const Keyframe& k : keys) keyList.push_back(k.Serialize());
    node["keys"] = keyList;
    node["wrap"] = static_cast<int>(wrap);
    return node;
}

void AnimationCurve::Deserialize(const YAML::Node& node) {
    keys.clear();
    wrap = Wrap::Clamp;
    if (!node) return;

    if (node["keys"] && node["keys"].IsSequence()) {
        for (const auto& kn : node["keys"]) {
            Keyframe k;
            k.Deserialize(kn);
            keys.push_back(k);
        }
    }
    wrap = static_cast<Wrap>(node["wrap"].as<int>(static_cast<int>(Wrap::Clamp)));

    // Trust nothing about the file's ordering -- the bracketing search in
    // Evaluate assumes sorted keys, and a hand-edited scene is entitled to be
    // wrong.
    Sort();
}

bool AnimationCurve::operator==(const AnimationCurve& o) const {
    if (wrap != o.wrap) return false;
    if (keys.size() != o.keys.size()) return false;
    for (size_t i = 0; i < keys.size(); ++i) {
        const Keyframe& a = keys[i];
        const Keyframe& b = o.keys[i];
        if (a.time != b.time || a.value != b.value
            || a.inTangent != b.inTangent || a.outTangent != b.outTangent) return false;
    }
    return true;
}
