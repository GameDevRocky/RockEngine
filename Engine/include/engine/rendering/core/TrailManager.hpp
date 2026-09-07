#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

#include <glm/glm.hpp>

class TrailRenderer;

// Process-global owner of every TrailRenderer's ribbon vertex buffer, living
// OUTSIDE any Container alongside Renderer / AssetManager / FontManager.
//
// Why it isn't just a member of TrailRenderer: play mode deep-copies the editor
// world and later destroys the copy. A GL handle held by a component would be
// duplicated by Copy() and then deleted twice, or leak. FontManager solved this
// exact problem for glyph meshes; this is the same shape.
//
// Keyed by the component's id, which Copy() preserves -- so the editor component
// and its play-mode twin share one entry. Only one container is active at a
// time, and the staleness test below is state-based rather than a dirty flag
// living on one particular copy, so the worst case is one extra rebuild per
// play/stop transition.
class TrailManager {
public:
    static TrailManager& Get();

    // Build or refresh this trail's mesh, returning its vertex count (0 means
    // nothing to draw -- fewer than two distinct points). Context must be
    // current.
    int EnsureMesh(TrailRenderer* trail, std::uint64_t frameId);

    // The buffer EnsureMesh filled, for binding. 0 if unknown.
    unsigned int GetVBO(const std::string& trailId) const;

    // Free buffers for trails not touched since `frameId` -- deleted components,
    // unloaded scenes, the discarded play-mode container. Context must be
    // current.
    void GarbageCollect(std::uint64_t frameId);

private:
    TrailManager() = default;
    ~TrailManager() = default;
    TrailManager(const TrailManager&) = delete;
    TrailManager& operator=(const TrailManager&) = delete;

    struct MeshState {
        unsigned int  vbo = 0;
        int           capacityVerts = 0;   // allocated, to avoid reallocating every frame
        int           vertexCount = 0;     // in use

        // Staleness is a revision counter plus the head position, NOT a content
        // hash like FontManager's. A trail's geometry changes on almost every
        // frame it is visible, so hashing its whole state would cost about what
        // rebuilding costs. This pair is enough to make the SECOND viewport in a
        // frame a cache hit, which is the case actually worth catching.
        std::uint64_t revision = ~0ull;
        glm::vec2     headPos{0.0f};
        bool          valid = false;

        std::uint64_t lastTouchedFrame = 0;
    };

    std::unordered_map<std::string, MeshState> meshes;
};
