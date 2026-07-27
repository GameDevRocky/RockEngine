#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

class Shader;

// ─────────────────────────────────────────────────────────────────────────────
// Per-light 1D polar depth maps, packed one light per row of a single texture.
//
// Each row is split into FOUR quadrants (+X, +Y, -X, -Y). That's a cube shadow
// map flattened to 2D: the four 90-degree faces sidestep the +/-pi wraparound
// that makes a single angle -> u mapping discontinuous, and each face is a plain
// perspective projection the GPU can rasterize and interpolate correctly.
//
// Deliberately NOT a singleton, unlike LightBuffer. It owns an FBO and a VAO,
// and neither is shared across a Qt context group -- one instance per
// LightingPass (i.e. per RenderView) keeps every GL object in the context that
// created it. See the note on Renderer::Blit for the same VAO trap.
// ─────────────────────────────────────────────────────────────────────────────
class ShadowAtlas
{
public:
    // Angular samples per 90-degree quadrant. The full row is 4x this.
    static constexpr int kResolution = 256;

    ShadowAtlas() = default;
    ~ShadowAtlas() = default;

    ShadowAtlas(const ShadowAtlas&) = delete;
    ShadowAtlas& operator=(const ShadowAtlas&) = delete;

    // Creates the texture/FBO/VAO and resolves the shadow_map shader. Idempotent.
    // Returns false if the shader is missing, in which case the caller must skip
    // shadow rendering entirely (lights still work, just unoccluded).
    bool EnsureInitialized(int maxShadowLights);

    // Bind the atlas FBO and clear every row to "no occluder" (far = 1.0).
    // Saves the caller's FBO/viewport/blend/scissor state; EndFrame restores it.
    void BeginFrame();

    // Upload the frame's occluder edges (flat world-space point PAIRS) ONCE.
    // The occluder set is a property of the scene, not of any one light, so this
    // is deliberately separate from RenderLight -- uploading per light re-sent the
    // whole buffer up to kMaxShadowLights times a frame, which stopped being
    // negligible the moment sprite-alpha silhouettes replaced 4-segment boxes.
    void SetOccluders(const std::vector<glm::vec2>& segments);

    // Rasterize the uploaded occluders into row `slot`.
    // Must be called between BeginFrame and EndFrame, after SetOccluders.
    void RenderLight(int slot, const glm::vec2& lightPos, float range);

    void EndFrame();

    // Bind the atlas texture to a texture unit for sampling in a lit shader.
    void Bind(int textureSlot) const;

    void Shutdown();

    GLuint GetTextureID() const { return m_texture; }
    bool   IsValid() const { return m_texture != 0 && m_shader != nullptr; }

private:
    GLuint m_fbo     = 0;
    GLuint m_texture = 0;
    GLuint m_vao     = 0;
    GLuint m_vbo     = 0;

    // Borrowed from AssetManager -- never deleted here (see RenderPass's
    // resource-ownership rule).
    Shader* m_shader = nullptr;

    int m_rows        = 0;   // = kMaxShadowLights
    int m_vertexCount = 0;   // verts currently in m_vbo
    std::size_t m_vboCapacityVerts = 0;

    std::vector<float> m_scratch;   // reused CPU staging buffer

    // Caller state captured by BeginFrame.
    GLint   m_prevFBO = 0;
    GLint   m_prevViewport[4] = { 0, 0, 0, 0 };
    GLboolean m_prevScissor = GL_FALSE;
    GLboolean m_prevBlend   = GL_FALSE;
    GLboolean m_prevDepthTest = GL_FALSE;
};
