#include "engine/rendering/core/LightBuffer.hpp"
#include <algorithm>
#include <cstring>

void LightBuffer::EnsureInitialized()
{
    if (m_initialized) return;

    glad_glGenBuffers(1, &m_ubo);
    glad_glBindBuffer(GL_UNIFORM_BUFFER, m_ubo);
    // Allocated at full size up front and refilled with glBufferSubData each
    // frame: a fixed-size block keeps the std140 layout constant, which is what
    // lets every lit shader declare the array once and never re-link.
    glad_glBufferData(GL_UNIFORM_BUFFER, sizeof(BlockData), nullptr, GL_DYNAMIC_DRAW);
    glad_glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glad_glBindBufferBase(GL_UNIFORM_BUFFER, LightLimits::kLightBlockBinding, m_ubo);

    m_initialized = true;
}

void LightBuffer::Upload(const std::vector<GpuLight>& lights, const glm::vec4& ambient)
{
    EnsureInitialized();

    BlockData data;
    data.ambient = ambient;

    const int count = static_cast<int>(
        std::min<std::size_t>(lights.size(), LightLimits::kMaxLights));
    data.meta = glm::ivec4(count, 0, 0, 0);

    if (count > 0)
        std::memcpy(data.lights, lights.data(), sizeof(GpuLight) * static_cast<std::size_t>(count));

    glad_glBindBuffer(GL_UNIFORM_BUFFER, m_ubo);
    // Only the header + the lights actually in use are uploaded. Stale entries
    // past `count` are never read -- the shader loops to uLightMeta.x.
    const GLsizeiptr bytes = static_cast<GLsizeiptr>(
        sizeof(glm::vec4) + sizeof(glm::ivec4) + sizeof(GpuLight) * static_cast<std::size_t>(count));
    glad_glBufferSubData(GL_UNIFORM_BUFFER, 0, bytes, &data);
    glad_glBindBuffer(GL_UNIFORM_BUFFER, 0);

    BindBase();
}

void LightBuffer::BindBase() const
{
    if (!m_ubo) return;
    glad_glBindBufferBase(GL_UNIFORM_BUFFER, LightLimits::kLightBlockBinding, m_ubo);
}

void LightBuffer::Shutdown()
{
    if (m_ubo) glad_glDeleteBuffers(1, &m_ubo);
    m_ubo = 0;
    m_initialized = false;
}
