#include "engine/rendering/core/TrailManager.hpp"

#include <vector>

#include <glad/glad.h>

#include "engine/components/TrailRenderer.hpp"
#include "engine/rendering/TrailMesh.hpp"

TrailManager& TrailManager::Get() {
    static TrailManager instance;
    return instance;
}

int TrailManager::EnsureMesh(TrailRenderer* trail, std::uint64_t frameId) {
    if (!trail) return 0;

    MeshState& st = meshes[trail->GetID()];
    st.lastTouchedFrame = frameId;

    const auto& points = trail->GetPoints();
    const glm::vec2 head = points.empty() ? glm::vec2(0.0f) : points.front().pos;

    if (st.valid && st.revision == trail->GetRevision() && st.headPos == head)
        return st.vertexCount;

    st.valid = true;
    st.revision = trail->GetRevision();
    st.headPos = head;

    TrailMeshSpec spec;
    spec.widthCurve = &trail->GetWidthCurve();
    spec.widthMultiplier = trail->GetWidthMultiplier();
    spec.startColor = trail->GetStartColor();
    spec.endColor = trail->GetEndColor();

    const std::vector<TrailVertex> mesh = TrailMesh::Build(points, spec);
    const int vertexCount = static_cast<int>(mesh.size());
    st.vertexCount = vertexCount;
    if (vertexCount == 0) return 0;

    // TrailVertex is { vec2, vec2, vec4 } of floats with no padding, so the
    // vector is already the exact interleaved layout the VAO describes and can
    // be uploaded without a repacking pass.
    static_assert(sizeof(TrailVertex) == TrailMesh::kFloatsPerVertex * sizeof(float),
                  "TrailVertex must stay tightly packed -- the VAO stride assumes it");

    if (st.vbo == 0) glGenBuffers(1, &st.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, st.vbo);

    const GLsizeiptr byteSize = static_cast<GLsizeiptr>(mesh.size() * sizeof(TrailVertex));
    if (vertexCount > st.capacityVerts) {
        // Grow, never shrink: a trail's length oscillates constantly as points
        // expire, and reallocating on every dip would churn the driver's
        // allocator for no gain.
        glBufferData(GL_ARRAY_BUFFER, byteSize, mesh.data(), GL_DYNAMIC_DRAW);
        st.capacityVerts = vertexCount;
    } else {
        glBufferSubData(GL_ARRAY_BUFFER, 0, byteSize, mesh.data());
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return vertexCount;
}

unsigned int TrailManager::GetVBO(const std::string& trailId) const {
    auto it = meshes.find(trailId);
    return it != meshes.end() ? it->second.vbo : 0u;
}

void TrailManager::GarbageCollect(std::uint64_t frameId) {
    // A grace period rather than "not touched this exact frame", for the reason
    // FontManager::GarbageCollect spells out: ScenePass runs once per scene per
    // viewport, so when scene 1 sweeps, scene 2's trails have not been drawn yet
    // this frame and an exact-match test would free buffers about to be redrawn.
    constexpr std::uint64_t kGraceFrames = 2;

    for (auto it = meshes.begin(); it != meshes.end(); ) {
        if (frameId > it->second.lastTouchedFrame + kGraceFrames) {
            if (it->second.vbo) glDeleteBuffers(1, &it->second.vbo);
            it = meshes.erase(it);
        } else {
            ++it;
        }
    }
}
