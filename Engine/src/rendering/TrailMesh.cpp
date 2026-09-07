#include "engine/rendering/TrailMesh.hpp"
#include "engine/utils/AnimationCurve.hpp"

#include <cmath>

namespace {

// Below this, two recorded points are the same point as far as the ribbon is
// concerned. Positions are in pixels (1 world unit == 1 pixel), so this is a
// thousandth of a pixel -- small enough never to merge points a user meant to
// keep, large enough that normalize() cannot produce NaN.
constexpr float kEpsilon = 1e-3f;

bool TryNormalize(const glm::vec2& v, glm::vec2& out) {
    const float len = std::sqrt(v.x * v.x + v.y * v.y);
    if (len < kEpsilon) return false;
    out = v / len;
    return true;
}

}  // namespace

std::vector<TrailVertex> TrailMesh::Build(const std::deque<TrailPoint>& points,
                                          const TrailMeshSpec& spec) {
    std::vector<TrailVertex> verts;
    if (points.size() < 2) return verts;

    // ── 1. Drop coincident points ───────────────────────────────────────────
    // A stationary object keeps recording at the same spot; those points would
    // contribute zero-area triangles and, worse, zero-length direction vectors.
    // Filtering here rather than mid-loop keeps every index below meaningful.
    std::vector<glm::vec2> path;
    path.reserve(points.size());
    path.push_back(points[0].pos);
    for (size_t i = 1; i < points.size(); ++i) {
        const glm::vec2 d = points[i].pos - path.back();
        if (std::sqrt(d.x * d.x + d.y * d.y) >= kEpsilon) path.push_back(points[i].pos);
    }
    const size_t n = path.size();
    if (n < 2) return verts;

    // ── 2. Cumulative distance from the head ────────────────────────────────
    std::vector<float> dist(n, 0.0f);
    for (size_t i = 1; i < n; ++i) {
        const glm::vec2 d = path[i] - path[i - 1];
        dist[i] = dist[i - 1] + std::sqrt(d.x * d.x + d.y * d.y);
    }
    const float total = dist[n - 1];
    if (total < kEpsilon) return verts;

    // ── 3. One rib (a left/right vertex pair) per point ─────────────────────
    struct Rib { TrailVertex left, right; };
    std::vector<Rib> ribs;
    ribs.reserve(n);

    // Carried forward so a direction that cannot be computed reuses the last
    // good one instead of collapsing the ribbon to zero width.
    glm::vec2 lastDir{1.0f, 0.0f};

    for (size_t i = 0; i < n; ++i) {
        glm::vec2 dir = lastDir;
        if (i == 0) {
            TryNormalize(path[1] - path[0], dir);
        } else if (i == n - 1) {
            TryNormalize(path[n - 1] - path[n - 2], dir);
        } else {
            // Average the incoming and outgoing directions so a corner is
            // mitred rather than producing two ribs at conflicting angles.
            glm::vec2 in{0.0f}, out{0.0f};
            const bool okIn  = TryNormalize(path[i] - path[i - 1], in);
            const bool okOut = TryNormalize(path[i + 1] - path[i], out);
            if (okIn && okOut) {
                // A perfect 180-degree reversal cancels to zero; keep the
                // incoming direction there rather than normalizing nothing.
                if (!TryNormalize(in + out, dir)) dir = in;
            } else if (okIn) {
                dir = in;
            } else if (okOut) {
                dir = out;
            }
        }
        lastDir = dir;

        const glm::vec2 perp{ -dir.y, dir.x };
        const float u = dist[i] / total;
        const float width = spec.widthCurve ? spec.widthCurve->Evaluate(u) : 1.0f;
        const float half = 0.5f * width * spec.widthMultiplier;
        const glm::vec4 color = glm::mix(spec.startColor, spec.endColor, u);

        Rib r;
        r.left  = TrailVertex{ path[i] + perp * half, glm::vec2(u, 0.0f), color };
        r.right = TrailVertex{ path[i] - perp * half, glm::vec2(u, 1.0f), color };
        ribs.push_back(r);
    }

    // ── 4. Two triangles per segment ────────────────────────────────────────
    verts.reserve((n - 1) * 6);
    for (size_t i = 0; i + 1 < n; ++i) {
        const Rib& a = ribs[i];
        const Rib& b = ribs[i + 1];

        verts.push_back(a.left);
        verts.push_back(a.right);
        verts.push_back(b.right);

        verts.push_back(a.left);
        verts.push_back(b.right);
        verts.push_back(b.left);
    }

    return verts;
}
