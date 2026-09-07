#pragma once
#include <deque>
#include <vector>
#include <glm/glm.hpp>

class AnimationCurve;

// Pure ribbon geometry for TrailRenderer -- no GL, no components, no engine
// singletons. TrailManager owns the buffer and the upload; this owns the maths.
//
// The split mirrors TextLayout / FontManager, and exists for the same reason:
// the interesting, easy-to-get-wrong part (degenerate segments, zero-length
// normals, the width taper) is then reachable without a GL context.

// One recorded position on the trail, with the time it was laid down so the
// component can expire it.
struct TrailPoint {
    glm::vec2 pos{0.0f};
    float     birthTime = 0.0f;
};

struct TrailMeshSpec {
    // Sampled at the normalized head-to-tail distance. Null means constant width.
    const AnimationCurve* widthCurve = nullptr;
    float     widthMultiplier = 16.0f;      // pixels (== world units)
    glm::vec4 startColor{1.0f};             // at the head
    glm::vec4 endColor{1.0f, 1.0f, 1.0f, 0.0f};   // at the tail
};

// Interleaved to match the trail VAO: { vec2 pos, vec2 uv, vec4 color }.
// Positions are WORLD space, so the draw uses an identity model matrix.
struct TrailVertex {
    glm::vec2 pos{0.0f};
    glm::vec2 uv{0.0f};
    glm::vec4 color{1.0f};
};

namespace TrailMesh {

// Floats per vertex in the emitted buffer. Kept here next to TrailVertex so the
// VAO stride and the struct can never drift apart.
inline constexpr int kFloatsPerVertex = 8;

// Build an unindexed triangle list (2 triangles per segment), matching the
// existing convention -- see FontManager for why an index buffer is not worth a
// second buffer per component.
//
// `points` runs HEAD (newest, index 0) to TAIL (oldest). Returns an empty vector
// rather than degenerate geometry for anything that cannot form a ribbon: fewer
// than two distinct points, or a total length of zero.
std::vector<TrailVertex> Build(const std::deque<TrailPoint>& points,
                               const TrailMeshSpec& spec);

}  // namespace TrailMesh
