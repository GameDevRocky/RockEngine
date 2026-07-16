#include "utils/TexturePreviewWidget.hpp"

#include "engine/rendering/core/AssetManager.hpp"
#include "engine/rendering/core/Texture2D.hpp"
#include "engine/rendering/core/Shader.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <QOpenGLContext>
#include <QPointer>

namespace {
// The engine's sprite shader — same one AssetThumbnails uses for its previews.
constexpr const char* kSpriteShaderId = "s1";
constexpr int kPreviewHeight = 180;
}

TexturePreviewWidget::TexturePreviewWidget(const std::string& textureId, QWidget* parent)
    : QOpenGLWidget(parent), m_textureId(textureId)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(kPreviewHeight);

    // Repaint when the sampler settings change, so the preview tracks the
    // Filtering/Wrap dropdowns sitting directly beneath it in the inspector.
    if (Texture2D* tex = AssetManager::Get().GetTexture(m_textureId)) {
        QPointer<TexturePreviewWidget> self(this);
        auto refresh = [self]() {
            if (!self) return false;  // widget destroyed: auto-unsubscribe
            self->update();
            return true;
        };
        tex->Subscribe(refresh, Texture2D::FILTER_CHANGED_EVENT);
        tex->Subscribe(refresh, Texture2D::WRAP_CHANGED_EVENT);
    }
}

TexturePreviewWidget::~TexturePreviewWidget()
{
    if (m_vao == 0 && m_vbo == 0) return;
    if (!context() || !context()->isValid()) return;
    makeCurrent();
    glad_glDeleteVertexArrays(1, &m_vao);
    glad_glDeleteBuffers(1, &m_vbo);
    doneCurrent();
}

void TexturePreviewWidget::initializeGL()
{
    // The scene view owns the shared context and is what loads glad. It is
    // constructed at startup, long before an inspector row exists, but bail
    // rather than call through null entry points if that ever stops holding.
    if (!glad_glGenVertexArrays) return;

    // Textures, buffers and shader programs are shared via AA_ShareOpenGLContexts,
    // but VAOs are container objects and are never shared — this one must be built
    // in our own context.
    const float verts[] = {
        -0.5f, -0.5f,  0.0f, 0.0f,
         0.5f, -0.5f,  1.0f, 0.0f,
         0.5f,  0.5f,  1.0f, 1.0f,
        -0.5f, -0.5f,  0.0f, 0.0f,
         0.5f,  0.5f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.0f, 1.0f
    };

    glad_glGenVertexArrays(1, &m_vao);
    glad_glGenBuffers(1, &m_vbo);
    glad_glBindVertexArray(m_vao);
    glad_glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glad_glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glad_glEnableVertexAttribArray(0);
    glad_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glad_glEnableVertexAttribArray(1);
    glad_glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                               (void*)(2 * sizeof(float)));
    glad_glBindVertexArray(0);
}

void TexturePreviewWidget::paintGL()
{
    glad_glClearColor(0.10f, 0.10f, 0.10f, 1.0f);
    glad_glClear(GL_COLOR_BUFFER_BIT);

    if (m_vao == 0) return;
    Texture2D* tex = AssetManager::Get().GetTexture(m_textureId);
    Shader* shader = AssetManager::Get().GetShader(kSpriteShaderId);
    if (!tex || !shader) return;
    if (tex->GetWidth() <= 0 || tex->GetHeight() <= 0 || height() <= 0) return;

    glad_glEnable(GL_BLEND);
    glad_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Fit the texture inside the widget without distorting it: the quad spans
    // NDC [-1,1] at scale 2, so shrink whichever axis would otherwise stretch.
    const float texAspect  = static_cast<float>(tex->GetWidth()) / static_cast<float>(tex->GetHeight());
    const float viewAspect = static_cast<float>(width()) / static_cast<float>(height());
    float sx = 2.f;
    float sy = 2.f;
    if (texAspect >= viewAspect) sy = 2.f * viewAspect / texAspect;
    else                         sx = 2.f * texAspect / viewAspect;

    shader->Bind();

    const glm::mat4 model = glm::scale(glm::mat4(1.f), glm::vec3(sx, sy, 1.f));
    const glm::mat4 ident = glm::mat4(1.f);
    const auto& au = shader->GetActiveUniforms();
    auto has = [&](const char* n) { return au.count(n) > 0; };

    if (has("uModel"))    shader->SetMat4("uModel",    model);
    if (has("uView"))     shader->SetMat4("uView",     ident);
    if (has("uProj"))     shader->SetMat4("uProj",     ident);
    if (has("uSize"))     shader->SetVec2("uSize",     glm::vec2(1.f));
    if (has("uPivot"))    shader->SetVec2("uPivot",    glm::vec2(0.f));
    if (has("uUVScale"))  shader->SetVec2("uUVScale",  glm::vec2(1.f));
    if (has("uUVOffset")) shader->SetVec2("uUVOffset", glm::vec2(0.f));
    if (has("uColor"))    shader->SetVec4("uColor",    glm::vec4(1.f));

    tex->Bind(0);
    if (has("uTexture")) shader->SetTexture("uTexture", 0);

    glad_glBindVertexArray(m_vao);
    glad_glDrawArrays(GL_TRIANGLES, 0, 6);
    glad_glBindVertexArray(0);
    glad_glUseProgram(0);
}
