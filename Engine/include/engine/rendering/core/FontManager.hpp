#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

class Font;
class TextRenderer;

// Process-global owner of every TextRenderer's glyph vertex buffer, living
// OUTSIDE any Container alongside Renderer / AssetManager / ParticleManager.
//
// Why it isn't just a member of TextRenderer: play mode deep-copies the editor
// world and later destroys the copy. A GL handle held by a component would be
// duplicated by Copy() and then deleted twice, or leak. ParticleManager solved
// this exact problem for emitter SSBOs; this is the same shape with a VBO.
//
// The cache is keyed by the component's id, which Copy() preserves -- so the
// editor component and its play-mode twin share one entry. That is fine and
// intended, because a glyph mesh is a pure function of the component's layout
// state, and staleness is decided by hashing that state rather than by a dirty
// flag living on one particular copy. Only one container is active at a time, so
// a divergent runtime edit costs at most one rebuild per play/stop transition.
class FontManager {
public:
    static FontManager& Get();

    // Vertex layout of every mesh this manages: interleaved
    // { vec2 position (text-local world units), vec2 uv (atlas) }.
    // Matches ScenePass's existing sprite attribute layout so the same VAO
    // format serves both.
    static constexpr int kFloatsPerVertex = 4;

    // Build or refresh this renderer's mesh, and return its vertex count
    // (0 means there is nothing to draw -- empty string, missing font, or a
    // string made entirely of whitespace). Rebuilds only when the component's
    // layout state or the font's atlas generation has changed.
    // Context must be current. `font` must already be uploaded.
    int EnsureMesh(TextRenderer* renderer, const Font* font, std::uint64_t frameId);

    // The buffer EnsureMesh filled, for binding. 0 if unknown.
    unsigned int GetVBO(const std::string& rendererId) const;

    // Free buffers for components not touched since `frameId` -- deleted
    // TextRenderers, unloaded scenes, the discarded play-mode container.
    // Context must be current.
    void GarbageCollect(std::uint64_t frameId);

private:
    FontManager() = default;
    ~FontManager() = default;
    FontManager(const FontManager&) = delete;
    FontManager& operator=(const FontManager&) = delete;

    struct MeshState {
        unsigned int  vbo = 0;
        int           capacityVerts = 0;   // allocated, to avoid reallocating on every edit
        int           vertexCount = 0;     // in use
        std::uint64_t contentHash = 0;
        std::uint64_t lastTouchedFrame = 0;
    };

    std::unordered_map<std::string, MeshState> meshes;
};
