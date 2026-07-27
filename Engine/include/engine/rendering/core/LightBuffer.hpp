#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// The shared std140 uniform block every lit shader reads.
//
// Lives outside any Container, alongside Renderer/AssetManager/ParticleManager:
// GPU buffers have no per-world identity, and the editor and runtime containers
// share one GL context. Unlike VAOs, buffer objects ARE shared across a context
// group (AA_ShareOpenGLContexts), so one process-wide UBO is correct here --
// see the comment on Renderer::Blit for the VAO case that is not.
//
// Uploaded once per scene per frame by LightingPass. ScenePass never touches it:
// binding point 0 stays bound, so any shader declaring `LightBlock` picks the
// data up with no per-draw work at all.
// ─────────────────────────────────────────────────────────────────────────────
namespace LightLimits {
    // Bumping either of these means editing the matching `const` in sprite.glsl
    // (and any other shader that declares LightBlock) -- std140 array sizes are
    // baked into the block layout, and a mismatch silently reads garbage.
    constexpr int kMaxLights       = 32;
    constexpr int kMaxShadowLights = 16;

    // Binding point for LightBlock. Note ParticleManager uses std430 binding 0
    // for its SSBO -- that's a different binding namespace (SHADER_STORAGE vs
    // UNIFORM), so the two do not collide.
    constexpr GLuint kLightBlockBinding = 0;
}

// One light as the GPU sees it. Every member is a vec4 so std140's alignment
// rules are unambiguous (a bare float or vec2 in here would silently pad to 16
// bytes and desync the C++ and GLSL views of the struct).
struct GpuLight
{
    // xy = world position (Point/Spot) or unit direction (Directional)
    // z  = range | w = LightType
    glm::vec4 posRange{ 0.0f };
    // rgb = colour | a = intensity
    glm::vec4 color{ 1.0f };
    // xy = unit cone direction | z = cos(innerAngle) | w = cos(outerAngle)
    glm::vec4 spot{ 0.0f };
    // x = innerRadius (0..1) | y = falloff exponent
    // z = shadow atlas row, -1 when this light casts none | w = shadowStrength
    glm::vec4 params{ 0.0f, 1.0f, -1.0f, 1.0f };
    // x = height above the sprite plane | y = normalInfluence | zw = reserved
    glm::vec4 params2{ 0.0f, 1.0f, 0.0f, 0.0f };
};

class LightBuffer
{
public:
    static LightBuffer& Get()
    {
        static LightBuffer instance;
        return instance;
    }

    // Idempotent: whichever viewport gets a GL context first does the work.
    void EnsureInitialized();

    // Replace the block contents. `lights` beyond kMaxLights is ignored -- the
    // caller (LightingPass) is responsible for prioritising before it gets here.
    // `ambient` is rgb colour * a intensity, already folded from every Global
    // light in the scene.
    void Upload(const std::vector<GpuLight>& lights, const glm::vec4& ambient);

    // Bind to the block's binding point. Cheap and idempotent; called once per
    // Upload so a freshly created shader program picks it up.
    void BindBase() const;

    void Shutdown();

    GLuint GetBufferID() const { return m_ubo; }

private:
    LightBuffer() = default;
    LightBuffer(const LightBuffer&) = delete;
    LightBuffer& operator=(const LightBuffer&) = delete;

    // Mirrors the GLSL block layout exactly:
    //   vec4 uAmbient; ivec4 uLightMeta; GpuLight uLights[kMaxLights];
    struct BlockData
    {
        glm::vec4 ambient{ 1.0f };
        // x = active light count. The remaining three ints are std140 padding
        // that GLSL requires anyway, so they are named rather than hidden.
        glm::ivec4 meta{ 0 };
        GpuLight lights[LightLimits::kMaxLights];
    };

    GLuint m_ubo = 0;
    bool   m_initialized = false;
};
