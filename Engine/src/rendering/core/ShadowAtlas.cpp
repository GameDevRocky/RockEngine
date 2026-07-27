#include "engine/rendering/core/ShadowAtlas.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/rendering/core/Shader.hpp"
#include <iostream>

bool ShadowAtlas::EnsureInitialized(int maxShadowLights)
{
    if (m_texture && m_shader) return true;

    if (!m_shader)
        m_shader = AssetManager::Get().GetShaderByName("shadow_map");
    if (!m_shader)
    {
        // Missing shader asset: shadows are skipped, lighting still works.
        return false;
    }

    if (m_texture) return true;

    m_rows = maxShadowLights > 0 ? maxShadowLights : 1;

    glad_glGenTextures(1, &m_texture);
    glad_glBindTexture(GL_TEXTURE_2D, m_texture);
    glad_glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, kResolution * 4, m_rows, 0,
                      GL_RED, GL_FLOAT, nullptr);
    // NEAREST on purpose: adjacent quadrants in a row are discontinuous
    // directions, so a linear tap straddling a quadrant seam would blend two
    // unrelated occluder distances into a bogus one.
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glad_glGenFramebuffers(1, &m_fbo);
    glad_glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glad_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture, 0);
    if (glad_glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "[ShadowAtlas] FBO not complete" << std::endl;
    glad_glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glad_glGenVertexArrays(1, &m_vao);
    glad_glGenBuffers(1, &m_vbo);
    glad_glBindVertexArray(m_vao);
    glad_glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glad_glEnableVertexAttribArray(0);  // world position
    glad_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glad_glEnableVertexAttribArray(1);  // side (-1 / +1)
    glad_glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)(2 * sizeof(float)));
    glad_glBindVertexArray(0);
    glad_glBindBuffer(GL_ARRAY_BUFFER, 0);

    return true;
}

void ShadowAtlas::BeginFrame()
{
    glad_glGetIntegerv(GL_FRAMEBUFFER_BINDING, &m_prevFBO);
    glad_glGetIntegerv(GL_VIEWPORT, m_prevViewport);
    m_prevScissor   = glad_glIsEnabled(GL_SCISSOR_TEST);
    m_prevBlend     = glad_glIsEnabled(GL_BLEND);
    m_prevDepthTest = glad_glIsEnabled(GL_DEPTH_TEST);

    glad_glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    // RenderPipeline may have left a scissor rect covering only the camera's
    // sub-region of the OUTPUT target. glClear honours scissor, so leaving it on
    // would clear only part of the atlas and leave last frame's occluders in the
    // rest.
    if (m_prevScissor) glad_glDisable(GL_SCISSOR_TEST);
    glad_glDisable(GL_DEPTH_TEST);

    glad_glViewport(0, 0, kResolution * 4, m_rows);
    // 1.0 == "nothing occludes out to the light's full range".
    glad_glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glad_glClear(GL_COLOR_BUFFER_BIT);

    // Nearest occluder wins. GL_MIN needs no depth buffer and no sorting, which
    // is why this pass has neither.
    glad_glEnable(GL_BLEND);
    glad_glBlendFunc(GL_ONE, GL_ONE);
    glad_glBlendEquation(GL_MIN);
}

void ShadowAtlas::SetOccluders(const std::vector<glm::vec2>& segments)
{
    // Expands each segment into a quad (6 verts) spanning the full clip-space
    // height, so the 1-pixel-tall row is fully covered. A GL_LINES primitive
    // would be at the mercy of the diamond-exit rule here and drop fragments.
    //
    // Two points per segment; six verts per segment; three floats per vert.
    const std::size_t segCount = segments.size() / 2;
    m_scratch.clear();
    m_scratch.reserve(segCount * 6 * 3);

    for (std::size_t i = 0; i + 1 < segments.size(); i += 2)
    {
        const glm::vec2& a = segments[i];
        const glm::vec2& b = segments[i + 1];
        const float verts[6][3] = {
            { a.x, a.y, -1.0f },
            { b.x, b.y, -1.0f },
            { b.x, b.y,  1.0f },
            { a.x, a.y, -1.0f },
            { b.x, b.y,  1.0f },
            { a.x, a.y,  1.0f },
        };
        for (const auto& v : verts)
        {
            m_scratch.push_back(v[0]);
            m_scratch.push_back(v[1]);
            m_scratch.push_back(v[2]);
        }
    }

    m_vertexCount = static_cast<int>(segCount * 6);
    if (m_vertexCount == 0) return;

    glad_glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    // Grow-only: reallocate on growth, refill in place otherwise. Occluder counts
    // are stable frame to frame, so this settles after the first frame.
    if (m_scratch.size() > m_vboCapacityVerts)
    {
        glad_glBufferData(GL_ARRAY_BUFFER,
                          static_cast<GLsizeiptr>(m_scratch.size() * sizeof(float)),
                          m_scratch.data(), GL_DYNAMIC_DRAW);
        m_vboCapacityVerts = m_scratch.size();
    }
    else
    {
        glad_glBufferSubData(GL_ARRAY_BUFFER, 0,
                             static_cast<GLsizeiptr>(m_scratch.size() * sizeof(float)),
                             m_scratch.data());
    }
    glad_glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void ShadowAtlas::RenderLight(int slot, const glm::vec2& lightPos, float range)
{
    if (!m_shader || slot < 0 || slot >= m_rows) return;
    if (m_vertexCount == 0) return;

    m_shader->Bind();
    m_shader->SetVec2("uLightPos", lightPos);
    m_shader->SetFloat("uRange", range);

    glad_glBindVertexArray(m_vao);
    for (int q = 0; q < 4; ++q)
    {
        // One row of one quadrant. Restricting the viewport is what confines a
        // 90-degree face to its own quarter of the row -- the projection below
        // maps the face to the full [-1,1] NDC x range regardless.
        glad_glViewport(q * kResolution, slot, kResolution, 1);
        m_shader->SetInt("uQuadrant", q);
        glad_glDrawArrays(GL_TRIANGLES, 0, m_vertexCount);
    }
    glad_glBindVertexArray(0);
}

void ShadowAtlas::EndFrame()
{
    glad_glBlendEquation(GL_FUNC_ADD);
    glad_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (!m_prevBlend) glad_glDisable(GL_BLEND);
    if (m_prevDepthTest) glad_glEnable(GL_DEPTH_TEST);
    if (m_prevScissor) glad_glEnable(GL_SCISSOR_TEST);

    glad_glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(m_prevFBO));
    glad_glViewport(m_prevViewport[0], m_prevViewport[1], m_prevViewport[2], m_prevViewport[3]);
    glad_glUseProgram(0);
}

void ShadowAtlas::Bind(int textureSlot) const
{
    if (!m_texture) return;
    glad_glActiveTexture(GL_TEXTURE0 + textureSlot);
    glad_glBindTexture(GL_TEXTURE_2D, m_texture);
}

void ShadowAtlas::Shutdown()
{
    if (m_fbo)     glad_glDeleteFramebuffers(1, &m_fbo);
    if (m_texture) glad_glDeleteTextures(1, &m_texture);
    if (m_vao)     glad_glDeleteVertexArrays(1, &m_vao);
    if (m_vbo)     glad_glDeleteBuffers(1, &m_vbo);
    m_fbo = m_texture = m_vao = m_vbo = 0;
    m_vboCapacityVerts = 0;
    m_vertexCount = 0;
    // Borrowed from AssetManager -- null it, never delete it.
    m_shader = nullptr;
}
