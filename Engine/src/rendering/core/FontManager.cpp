#include "engine/rendering/core/FontManager.hpp"

#include <cstring>
#include <vector>

#include <glad/glad.h>

#include "engine/components/TextRenderer.hpp"
#include "engine/rendering/core/Font.hpp"
#include "engine/text/TextLayout.hpp"

namespace {

void HashCombine(std::uint64_t& seed, std::uint64_t value) {
    value += 0x9E3779B97F4A7C15ull;
    value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ull;
    value = (value ^ (value >> 27)) * 0x94D049BB133111EBull;
    seed ^= value ^ (seed << 6) ^ (seed >> 2);
}

void HashFloat(std::uint64_t& seed, float f) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &f, sizeof(bits));
    HashCombine(seed, bits);
}

// Everything the vertex data depends on -- and deliberately nothing else.
// Colour, weight, and the outline are uniforms, so folding them in here would
// re-run layout and re-upload a buffer on every drag of a colour picker.
std::uint64_t ContentHash(const TextRenderer* r, const Font* font) {
    std::uint64_t h = 0xCBF29CE484222325ull;

    for (char c : r->GetText()) HashCombine(h, static_cast<std::uint64_t>(c));
    HashCombine(h, r->GetText().size());
    for (char c : r->GetFontID()) HashCombine(h, static_cast<std::uint64_t>(c));

    // A re-bake repacks the atlas and moves every glyph, invalidating the UVs
    // baked into these vertices.
    HashCombine(h, font ? font->GetAtlasGeneration() : 0u);

    const TextLayoutSpec spec = r->GetLayoutSpec();
    HashFloat(h, spec.fontSize);
    HashFloat(h, spec.lineSpacing);
    HashFloat(h, spec.letterSpacing);
    HashFloat(h, spec.maxWidth);
    HashCombine(h, static_cast<std::uint64_t>(spec.hAlign));
    HashCombine(h, static_cast<std::uint64_t>(spec.vAlign));
    return h;
}

} // namespace

FontManager& FontManager::Get() {
    static FontManager instance;
    return instance;
}

// ─────────────────────────────────────────────────────────────────────────────
int FontManager::EnsureMesh(TextRenderer* renderer, const Font* font,
                            std::uint64_t frameId) {
    if (!renderer) return 0;

    MeshState& st = meshes[renderer->GetID()];
    st.lastTouchedFrame = frameId;

    const std::uint64_t hash = ContentHash(renderer, font);
    if (st.vbo != 0 && st.contentHash == hash) return st.vertexCount;

    st.contentHash = hash;

    const TextMesh mesh = TextLayout::Build(font, renderer->GetText(),
                                            renderer->GetLayoutSpec());
    const int vertexCount = static_cast<int>(mesh.quads.size()) * 6;
    st.vertexCount = vertexCount;
    if (vertexCount == 0) return 0;

    // Two triangles per glyph, unindexed -- same as the sprite quad. An index
    // buffer would save a third of the vertex bandwidth on text that is never
    // the bottleneck, at the cost of a second buffer per component.
    std::vector<float> verts;
    verts.reserve(static_cast<size_t>(vertexCount) * kFloatsPerVertex);

    auto push = [&verts](float x, float y, float u, float v) {
        verts.push_back(x); verts.push_back(y); verts.push_back(u); verts.push_back(v);
    };

    for (const TextQuad& q : mesh.quads) {
        push(q.min.x, q.min.y, q.uvMin.x, q.uvMin.y);
        push(q.max.x, q.min.y, q.uvMax.x, q.uvMin.y);
        push(q.max.x, q.max.y, q.uvMax.x, q.uvMax.y);

        push(q.min.x, q.min.y, q.uvMin.x, q.uvMin.y);
        push(q.max.x, q.max.y, q.uvMax.x, q.uvMax.y);
        push(q.min.x, q.max.y, q.uvMin.x, q.uvMax.y);
    }

    if (st.vbo == 0) glGenBuffers(1, &st.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, st.vbo);

    const GLsizeiptr byteSize = static_cast<GLsizeiptr>(verts.size() * sizeof(float));
    if (vertexCount > st.capacityVerts) {
        // Grow (never shrink): editing text is a stream of small changes, and
        // reallocating on each one would churn the driver's allocator for no gain.
        glBufferData(GL_ARRAY_BUFFER, byteSize, verts.data(), GL_DYNAMIC_DRAW);
        st.capacityVerts = vertexCount;
    } else {
        glBufferSubData(GL_ARRAY_BUFFER, 0, byteSize, verts.data());
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return vertexCount;
}

unsigned int FontManager::GetVBO(const std::string& rendererId) const {
    auto it = meshes.find(rendererId);
    return it != meshes.end() ? it->second.vbo : 0u;
}

void FontManager::GarbageCollect(std::uint64_t frameId) {
    // Deliberately a grace period rather than "not touched this exact frame".
    // ScenePass runs once per scene per viewport, so at the moment scene 1
    // sweeps, scene 2's TextRenderers have not been drawn yet this frame -- an
    // exact-match test would free their buffers and force a rebuild every single
    // frame. Anything genuinely gone stays untouched and is collected a couple of
    // frames later, which is soon enough for a buffer measured in kilobytes.
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
