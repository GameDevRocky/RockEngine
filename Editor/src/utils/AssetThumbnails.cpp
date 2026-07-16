#include "utils/AssetThumbnails.hpp"
#include "dock-widgets/SceneViewGui.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/rendering/core/Material.hpp"
#include "engine/rendering/core/Sprite.hpp"
#include "engine/rendering/core/Texture2D.hpp"
#include "engine/rendering/core/Shader.hpp"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <QImage>

// ─── GL helpers (local to this TU) ───────────────────────────────────────────

static void makeQuad(GLuint& vao, GLuint& vbo)
{
    float verts[] = {
        -0.5f, -0.5f,  0.0f, 0.0f,
         0.5f, -0.5f,  1.0f, 0.0f,
         0.5f,  0.5f,  1.0f, 1.0f,
        -0.5f, -0.5f,  0.0f, 0.0f,
         0.5f,  0.5f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.0f, 1.0f
    };
    glad_glGenVertexArrays(1, &vao);
    glad_glGenBuffers(1, &vbo);
    glad_glBindVertexArray(vao);
    glad_glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glad_glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glad_glEnableVertexAttribArray(0);
    glad_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glad_glEnableVertexAttribArray(1);
    glad_glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                               (void*)(2 * sizeof(float)));
    glad_glBindVertexArray(0);
}

static bool beginFBO(int sz, GLuint& fbo, GLuint& colorTex,
                     GLint& prevFBO, GLint prevVp[4])
{
    glad_glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
    glad_glGetIntegerv(GL_VIEWPORT, prevVp);

    glad_glGenFramebuffers(1, &fbo);
    glad_glGenTextures(1, &colorTex);
    glad_glBindTexture(GL_TEXTURE_2D, colorTex);
    glad_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, sz, sz, 0,
                      GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glad_glBindTexture(GL_TEXTURE_2D, 0);

    glad_glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glad_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                GL_TEXTURE_2D, colorTex, 0);

    if (glad_glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glad_glDeleteFramebuffers(1, &fbo);
        glad_glDeleteTextures(1, &colorTex);
        glad_glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prevFBO);
        return false;
    }

    glad_glViewport(0, 0, sz, sz);
    glad_glEnable(GL_BLEND);
    glad_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glad_glClearColor(0.f, 0.f, 0.f, 0.f);
    glad_glClear(GL_COLOR_BUFFER_BIT);
    return true;
}

static QPixmap endFBO(int sz, GLuint fbo, GLuint colorTex,
                      GLint prevFBO, const GLint prevVp[4])
{
    QImage img(sz, sz, QImage::Format_RGBA8888);
    glad_glReadPixels(0, 0, sz, sz, GL_RGBA, GL_UNSIGNED_BYTE, img.bits());
    img = img.mirrored(false, true);
    glad_glDeleteFramebuffers(1, &fbo);
    glad_glDeleteTextures(1, &colorTex);
    glad_glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prevFBO);
    glad_glViewport(prevVp[0], prevVp[1], prevVp[2], prevVp[3]);
    return QPixmap::fromImage(img);
}

// Renders a texture — or a sub-rect of one — into a square thumbnail using the
// engine's sprite shader. Shared by forSprite and forTexture: a whole-texture
// thumbnail is just the full [0,1] UV range with no pivot offset.
static QPixmap renderTextureQuad(int sz, Texture2D* tex, glm::vec2 uvMin,
                                 glm::vec2 uvMax, glm::vec2 pivot)
{
    Shader* shader = AssetManager::Get().GetShader("s1");
    if (!tex || !shader) return {};

    SceneViewGui* sv = SceneViewGui::Get();
    if (!sv || !sv->context() || !sv->context()->isValid()) return {};
    sv->makeCurrent();

    GLuint vao = 0, vbo = 0;
    makeQuad(vao, vbo);

    GLuint fbo = 0, colorTex = 0;
    GLint prevFBO = 0, prevVp[4] = {};
    if (!beginFBO(sz, fbo, colorTex, prevFBO, prevVp)) {
        glad_glDeleteVertexArrays(1, &vao);
        glad_glDeleteBuffers(1, &vbo);
        sv->doneCurrent();
        return {};
    }

    // Letterbox into the square target so the source pixels keep their aspect.
    const glm::vec2 uvScale = uvMax - uvMin;
    float aspect = 1.f;
    if (tex->GetWidth() > 0 && tex->GetHeight() > 0) {
        float pw = uvScale.x * static_cast<float>(tex->GetWidth());
        float ph = uvScale.y * static_cast<float>(tex->GetHeight());
        if (ph > 0.f) aspect = pw / ph;
    }
    float sx = (aspect >= 1.f) ? 2.f : 2.f * aspect;
    float sy = (aspect >= 1.f) ? 2.f / aspect : 2.f;

    shader->Bind();

    const glm::mat4 model = glm::scale(glm::mat4(1.f), glm::vec3(sx, sy, 1.f));
    const glm::mat4 ident = glm::mat4(1.f);
    const auto& au = shader->GetActiveUniforms();
    auto has = [&](const char* n) { return au.count(n) > 0; };

    if (has("uModel"))    shader->SetMat4("uModel",    model);
    if (has("uView"))     shader->SetMat4("uView",     ident);
    if (has("uProj"))     shader->SetMat4("uProj",     ident);
    if (has("uSize"))     shader->SetVec2("uSize",     glm::vec2(1.f));
    if (has("uPivot"))    shader->SetVec2("uPivot",    pivot);
    if (has("uUVScale"))  shader->SetVec2("uUVScale",  uvScale);
    if (has("uUVOffset")) shader->SetVec2("uUVOffset", uvMin);
    if (has("uColor"))    shader->SetVec4("uColor",    glm::vec4(1.f));

    tex->Bind(0);
    if (has("uTexture")) shader->SetTexture("uTexture", 0);

    glad_glBindVertexArray(vao);
    glad_glDrawArrays(GL_TRIANGLES, 0, 6);
    glad_glBindVertexArray(0);
    glad_glUseProgram(0);

    QPixmap result = endFBO(sz, fbo, colorTex, prevFBO, prevVp);
    glad_glDeleteVertexArrays(1, &vao);
    glad_glDeleteBuffers(1, &vbo);
    sv->doneCurrent();
    return result;
}

// ─── Public API ──────────────────────────────────────────────────────────────

namespace AssetThumbnails {

QPixmap forMaterial(const std::string& id)
{
    Material* mat = AssetManager::Get().GetMaterial(id);
    if (!mat) return {};
    Shader* shader = mat->GetShader();
    if (!shader) return {};

    SceneViewGui* sv = SceneViewGui::Get();
    if (!sv || !sv->context() || !sv->context()->isValid()) return {};
    sv->makeCurrent();

    GLuint vao = 0, vbo = 0;
    makeQuad(vao, vbo);

    GLuint fbo = 0, colorTex = 0;
    GLint prevFBO = 0, prevVp[4] = {};
    if (!beginFBO(kSize, fbo, colorTex, prevFBO, prevVp)) {
        glad_glDeleteVertexArrays(1, &vao);
        glad_glDeleteBuffers(1, &vbo);
        sv->doneCurrent();
        return {};
    }

    float aspect = 1.f;
    for (const auto& [uname, texId] : mat->GetTexUniforms()) {
        Texture2D* t = AssetManager::Get().GetTexture(texId);
        if (t && t->GetWidth() > 0 && t->GetHeight() > 0) {
            aspect = static_cast<float>(t->GetWidth()) / static_cast<float>(t->GetHeight());
            break;
        }
    }
    float sx = (aspect >= 1.f) ? 2.f : 2.f * aspect;
    float sy = (aspect >= 1.f) ? 2.f / aspect : 2.f;

    shader->Bind();
    mat->ApplyUniforms();

    const glm::mat4 model = glm::scale(glm::mat4(1.f), glm::vec3(sx, sy, 1.f));
    const glm::mat4 ident = glm::mat4(1.f);
    const auto& au = shader->GetActiveUniforms();
    auto has = [&](const char* n) { return au.count(n) > 0; };

    if (has("uModel")) shader->SetMat4("uModel", model);
    if (has("uView"))  shader->SetMat4("uView",  ident);
    if (has("uProj"))  shader->SetMat4("uProj",  ident);
    if (has("uSize"))  shader->SetVec2("uSize",  glm::vec2(1.f));
    if (has("uPivot")) shader->SetVec2("uPivot", glm::vec2(0.f));

    glad_glBindVertexArray(vao);
    glad_glDrawArrays(GL_TRIANGLES, 0, 6);
    glad_glBindVertexArray(0);
    glad_glUseProgram(0);

    QPixmap result = endFBO(kSize, fbo, colorTex, prevFBO, prevVp);
    glad_glDeleteVertexArrays(1, &vao);
    glad_glDeleteBuffers(1, &vbo);
    sv->doneCurrent();
    return result;
}

QPixmap forSprite(const std::string& id)
{
    Sprite* sprite = AssetManager::Get().GetSprite(id);
    if (!sprite) return {};
    return renderTextureQuad(kSize, sprite->GetTexture(), sprite->GetUVMin(),
                             sprite->GetUVMax(), sprite->GetPivot());
}

QPixmap forTexture(const std::string& id)
{
    return renderTextureQuad(kSize, AssetManager::Get().GetTexture(id),
                             glm::vec2(0.f), glm::vec2(1.f), glm::vec2(0.f));
}

}