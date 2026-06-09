#include "utils/AssetPreviewDelegate.hpp"
#include "dock-widgets/SceneViewGui.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/rendering/core/Material.hpp"
#include "engine/rendering/core/Shader.hpp"
#include "engine/rendering/core/Texture2D.hpp"
#include "engine/rendering/core/Sprite.hpp"
#include "engine/rendering/core/Resource.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <QPainter>
#include <QFileInfo>
#include <QSortFilterProxyModel>
#include <QStyleOptionViewItem>
#include <QMetaObject>
#include <QApplication>
#include <QStyle>

#include <yaml-cpp/yaml.h>
#include <iostream>
#include <any>

// ──────────────────────────────────────────────────────────────────────────────
// Internal helper: read a string field from a YAML file, return "" on error.
static std::string yamlField(const QString& filePath, const char* key) {
    try {
        YAML::Node n = YAML::LoadFile(filePath.toStdString());
        if (n[key]) return n[key].as<std::string>();
    } catch (...) {}
    return {};
}

// Resolve the source image path for a .texture meta file.
// The meta file has a "path" field that is relative to PROJECT_ROOT.
static QString textureSourcePath(const QString& metaPath) {
    std::string rel = yamlField(metaPath, "path");
    if (rel.empty()) return {};
    QString abs = QString(PROJECT_ROOT) + "/" + QString::fromStdString(rel);
    return QFileInfo::exists(abs) ? abs : QString();
}

// ──────────────────────────────────────────────────────────────────────────────
AssetPreviewDelegate::AssetPreviewDelegate(QFileSystemModel* fsModel,
                                            QAbstractItemView* view,
                                            QObject* parent)
    : QStyledItemDelegate(parent), m_fsModel(fsModel), m_view(view)
{
    refreshSubscriptions();

    m_managerSubId = AssetManager::Get().Subscribe(
        [this](std::any data) -> bool {
            if (data.has_value())
                try { subscribeToAsset(std::any_cast<Resource*>(data)); }
                catch (const std::bad_any_cast&) {}
            return true;
        }, AssetManager::ASSET_ADDED_EVENT);
}

AssetPreviewDelegate::~AssetPreviewDelegate() {
    if (m_managerSubId >= 0)
        AssetManager::Get().Unsubscribe(m_managerSubId);

    for (auto& [asset, ids] : m_subscriptions)
        for (int id : ids)
            asset->Unsubscribe(id);

    // GL resources were created inside SceneView's context; only clean up if
    // the context is still valid and current.
    if (m_quadVAO) {
        SceneViewGui* sv = SceneViewGui::Get();
        if (sv && sv->context() && sv->context()->isValid()) {
            sv->makeCurrent();
            glad_glDeleteVertexArrays(1, &m_quadVAO);
            glad_glDeleteBuffers(1, &m_quadVBO);
            sv->doneCurrent();
        }
    }
}

// ──────────────────────────────────────────────────────────────────────────────
QString AssetPreviewDelegate::filePathForIndex(const QModelIndex& index) const {
    if (!index.isValid()) return {};
    // The view may be using a proxy model — map to source before asking fsModel.
    if (auto* proxy = qobject_cast<const QSortFilterProxyModel*>(index.model())) {
        return m_fsModel->filePath(proxy->mapToSource(index));
    }
    return m_fsModel->filePath(index);
}

// ──────────────────────────────────────────────────────────────────────────────
QSize AssetPreviewDelegate::sizeHint(const QStyleOptionViewItem& option,
                                      const QModelIndex& index) const {
    Q_UNUSED(option); Q_UNUSED(index);
    return QSize(100, 110);
}

// ──────────────────────────────────────────────────────────────────────────────
void AssetPreviewDelegate::paint(QPainter* painter,
                                  const QStyleOptionViewItem& option,
                                  const QModelIndex& index) const {
    if (!index.isValid()) { QStyledItemDelegate::paint(painter, option, index); return; }

    const QString filePath = filePathForIndex(index);
    const QString ext      = QFileInfo(filePath).suffix().toLower();

    // ── .shader meta: show default icon with clean label (no extension) ──────────
    if (ext == "shader") {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        // Strip double extension: "sprite.glsl.shader" → "sprite.glsl" → "sprite"
        QString stem = QFileInfo(filePath).completeBaseName(); // "sprite.glsl"
        opt.text = QFileInfo(stem).completeBaseName();          // "sprite"
        QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &opt, painter, m_view);
        return;
    }

    // ── Asset types that get a pixmap/GL thumbnail ────────────────────────────
    const bool needsThumb = (ext == "texture" || ext == "mat" ||
                             ext == "material" || ext == "sprite");
    if (!needsThumb) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    // ── Render fresh thumbnail every paint (no caching) ───────────────────────
    QPixmap px;
    if (ext == "mat" || ext == "material") {
        px = renderMaterialPreview(filePath);
    } else if (ext == "texture") {
        QString src = textureSourcePath(filePath);
        if (!src.isEmpty()) px = QPixmap(src);
    } else if (ext == "sprite") {
        px = renderSpritePreview(filePath);
    }

    if (px.isNull()) {
        px = QPixmap(kThumbSize, kThumbSize);
        px.fill(QColor(80, 80, 80));
    }

    QString label = QFileInfo(filePath).fileName();

    drawCell(painter, option, px, label);
}

// ──────────────────────────────────────────────────────────────────────────────
void AssetPreviewDelegate::drawCell(QPainter* p,
                                     const QStyleOptionViewItem& opt,
                                     const QPixmap& thumb,
                                     const QString& label) const {
    p->save();

    // Selection highlight
    if (opt.state & QStyle::State_Selected) {
        p->fillRect(opt.rect, opt.palette.highlight());
    }

    // Thumbnail centred in top portion of cell
    QRect iconArea(opt.rect.left() + (opt.rect.width() - kThumbSize) / 2,
                   opt.rect.top() + 8,
                   kThumbSize, kThumbSize);
    if (!thumb.isNull()) {
        QPixmap scaled = thumb.scaled(kThumbSize, kThumbSize,
                                      Qt::KeepAspectRatio,
                                      Qt::SmoothTransformation);
        QRect dst(iconArea.left() + (iconArea.width()  - scaled.width())  / 2,
                  iconArea.top()  + (iconArea.height() - scaled.height()) / 2,
                  scaled.width(), scaled.height());
        p->drawPixmap(dst, scaled);
    }

    // Label below the icon
    QRect textArea(opt.rect.left(),
                   opt.rect.top() + kThumbSize + 12,
                   opt.rect.width(),
                   opt.rect.height() - kThumbSize - 12);
    QPen textPen = (opt.state & QStyle::State_Selected)
                       ? QPen(opt.palette.highlightedText().color())
                       : QPen(opt.palette.text().color());
    p->setPen(textPen);
    p->drawText(textArea,
                Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
                label);

    p->restore();
}

// ──────────────────────────────────────────────────────────────────────────────
void AssetPreviewDelegate::ensureQuadGeometry() const {
    if (m_quadVAO) return;

    // Matches the vertex layout used by ScenePass:
    // location 0: vec2 pos  (range [-0.5, 0.5])
    // location 1: vec2 uv   (range [0, 1])
    float verts[] = {
        -0.5f, -0.5f,  0.0f, 0.0f,
         0.5f, -0.5f,  1.0f, 0.0f,
         0.5f,  0.5f,  1.0f, 1.0f,
        -0.5f, -0.5f,  0.0f, 0.0f,
         0.5f,  0.5f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.0f, 1.0f
    };

    glad_glGenVertexArrays(1, &m_quadVAO);
    glad_glGenBuffers(1, &m_quadVBO);

    glad_glBindVertexArray(m_quadVAO);
    glad_glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glad_glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    glad_glEnableVertexAttribArray(0);
    glad_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    glad_glEnableVertexAttribArray(1);
    glad_glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                               (void*)(2 * sizeof(float)));

    glad_glBindVertexArray(0);
}

// ──────────────────────────────────────────────────────────────────────────────
// Shared FBO render logic used by both sprite and material previews.
// Caller is responsible for binding the shader, setting all uniforms, and
// binding/unbinding the quad VAO around the draw call.
static bool beginFBO(int size, GLuint& fbo, GLuint& colorTex,
                     GLint& prevFBO, GLint prevViewport[4]) {
    glad_glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
    glad_glGetIntegerv(GL_VIEWPORT, prevViewport);

    glad_glGenFramebuffers(1, &fbo);
    glad_glGenTextures(1, &colorTex);

    glad_glBindTexture(GL_TEXTURE_2D, colorTex);
    glad_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0,
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

    glad_glViewport(0, 0, size, size);
    glad_glEnable(GL_BLEND);
    glad_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glad_glClearColor(0.f, 0.f, 0.f, 0.f);
    glad_glClear(GL_COLOR_BUFFER_BIT);
    return true;
}

static QPixmap readFBO(int size, GLuint fbo, GLuint colorTex,
                        GLint prevFBO, const GLint prevViewport[4]) {
    QImage img(size, size, QImage::Format_RGBA8888);
    glad_glReadPixels(0, 0, size, size, GL_RGBA, GL_UNSIGNED_BYTE, img.bits());
    img = img.mirrored(false, true);

    glad_glDeleteFramebuffers(1, &fbo);
    glad_glDeleteTextures(1, &colorTex);
    glad_glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prevFBO);
    glad_glViewport(prevViewport[0], prevViewport[1],
                    prevViewport[2], prevViewport[3]);

    return QPixmap::fromImage(img);
}

// ──────────────────────────────────────────────────────────────────────────────
void AssetPreviewDelegate::subscribeToAsset(Resource* asset) {
    if (!asset || m_subscriptions.count(asset)) return;

    auto trigger = [this, asset](std::any) -> bool {
        repaintAsset(asset);
        return true;
    };

    std::vector<int> ids;
    if (auto* mat = dynamic_cast<Material*>(asset)) {
        ids.push_back(mat->Subscribe(trigger, Material::SHADER_CHANGED_EVENT));
        ids.push_back(mat->Subscribe(trigger, Material::UNIFORM_CHANGED_EVENT));
    } else if (auto* sprite = dynamic_cast<Sprite*>(asset)) {
        ids.push_back(sprite->Subscribe(trigger, Sprite::UV_MIN_CHANGED_EVENT));
        ids.push_back(sprite->Subscribe(trigger, Sprite::UV_MAX_CHANGED_EVENT));
        ids.push_back(sprite->Subscribe(trigger, Sprite::PIVOT_CHANGED_EVENT));
        ids.push_back(sprite->Subscribe(trigger, Sprite::TEXTURE_CHANGED_EVENT));
    }

    if (!ids.empty())
        m_subscriptions[asset] = std::move(ids);
}

void AssetPreviewDelegate::refreshSubscriptions() {
    for (auto& [id, mat] : AssetManager::Get().GetAllMaterials())
        subscribeToAsset(mat);
    for (auto& [id, sprite] : AssetManager::Get().GetAllSprites())
        subscribeToAsset(sprite);
}

void AssetPreviewDelegate::repaintAsset(Resource* asset) {
    const std::string& fp = asset->GetFilePath();
    if (fp.empty()) return;
    QModelIndex src = m_fsModel->index(QString::fromStdString(fp));
    if (!src.isValid()) return;
    auto* proxy = qobject_cast<QSortFilterProxyModel*>(m_view->model());
    QModelIndex proxyIdx = proxy ? proxy->mapFromSource(src) : src;
    if (proxyIdx.isValid())
        m_view->update(proxyIdx);
}

// ──────────────────────────────────────────────────────────────────────────────
QPixmap AssetPreviewDelegate::renderSpritePreview(const QString& spritePath) const {
    Sprite* sprite = nullptr;
    try {
        YAML::Node node = YAML::LoadFile(spritePath.toStdString());
        if (node["id"])
            sprite = AssetManager::Get().GetSprite(node["id"].as<std::string>());
        if (!sprite && node["name"])
            sprite = AssetManager::Get().GetSpriteByName(node["name"].as<std::string>());
    } catch (const std::exception& e) {
        std::cerr << "AssetPreviewDelegate: sprite parse failed: " << e.what() << std::endl;
        return {};
    }

    if (!sprite) return {};

    Texture2D* tex    = sprite->GetTexture();
    Shader*    shader = AssetManager::Get().GetShader("s1");
    if (!tex || !shader) return {};

    const glm::vec2 uvMin  = sprite->GetUVMin();
    const glm::vec2 uvMax  = sprite->GetUVMax();
    const glm::vec2 pivot  = sprite->GetPivot();

    // Compute pixel dimensions of the sprite region to preserve aspect ratio.
    float aspect = 1.f;
    if (tex->GetWidth() > 0 && tex->GetHeight() > 0) {
        glm::vec2 uvScale = uvMax - uvMin;
        float pw = uvScale.x * static_cast<float>(tex->GetWidth());
        float ph = uvScale.y * static_cast<float>(tex->GetHeight());
        if (ph > 0.f) aspect = pw / ph;
    }
    float sx = (aspect >= 1.f) ? 2.f : 2.f * aspect;
    float sy = (aspect >= 1.f) ? 2.f / aspect : 2.f;

    SceneViewGui* sv = SceneViewGui::Get();
    if (!sv || !sv->context() || !sv->context()->isValid()) return {};
    sv->makeCurrent();

    ensureQuadGeometry();

    GLuint fbo = 0, colorTex = 0;
    GLint  prevFBO = 0, prevViewport[4] = {};
    if (!beginFBO(kThumbSize, fbo, colorTex, prevFBO, prevViewport)) {
        sv->doneCurrent();
        return {};
    }

    shader->Bind();

    const glm::mat4 model = glm::scale(glm::mat4(1.f), glm::vec3(sx, sy, 1.f));
    const glm::mat4 ident = glm::mat4(1.f);

    const auto& activeUniforms = shader->GetActiveUniforms();
    auto has = [&](const char* name){ return activeUniforms.count(name) > 0; };

    if (has("uModel"))     shader->SetMat4("uModel",     model);
    if (has("uView"))      shader->SetMat4("uView",      ident);
    if (has("uProj"))      shader->SetMat4("uProj",      ident);
    if (has("uSize"))      shader->SetVec2("uSize",      glm::vec2(1.f, 1.f));
    if (has("uPivot"))     shader->SetVec2("uPivot",     pivot);
    if (has("uUVScale"))   shader->SetVec2("uUVScale",   uvMax - uvMin);
    if (has("uUVOffset"))  shader->SetVec2("uUVOffset",  uvMin);
    if (has("uColor"))     shader->SetVec4("uColor",     glm::vec4(1.f));

    tex->Bind(0);
    if (has("uTexture"))   shader->SetTexture("uTexture", 0);

    glad_glBindVertexArray(m_quadVAO);
    glad_glDrawArrays(GL_TRIANGLES, 0, 6);
    glad_glBindVertexArray(0);
    glad_glUseProgram(0);

    QPixmap result = readFBO(kThumbSize, fbo, colorTex, prevFBO, prevViewport);
    sv->doneCurrent();
    return result;
}

// ──────────────────────────────────────────────────────────────────────────────
QPixmap AssetPreviewDelegate::renderMaterialPreview(const QString& matPath) const {
    // Resolve the material from AssetManager (it must already be loaded).
    Material* mat = nullptr;
    try {
        YAML::Node node = YAML::LoadFile(matPath.toStdString());
        if (node["id"]) {
            std::string id = node["id"].as<std::string>();
            mat = AssetManager::Get().GetMaterial(id);
        }
        // Fallback: try by name
        if (!mat && node["name"]) {
            mat = AssetManager::Get().GetMaterialByName(node["name"].as<std::string>());
        }
    } catch (const std::exception& e) {
        std::cerr << "AssetPreviewDelegate: failed to parse " << matPath.toStdString()
                  << " – " << e.what() << std::endl;
        return {};
    }

    if (!mat) return {};

    Shader* shader = mat->GetShader();
    if (!shader) return {};

    // ── Acquire the shared GL context ─────────────────────────────────────────
    SceneViewGui* sv = SceneViewGui::Get();
    if (!sv || !sv->context() || !sv->context()->isValid()) return {};

    sv->makeCurrent();

    // ── Lazy-init shared quad geometry ────────────────────────────────────────
    ensureQuadGeometry();

    GLuint fbo = 0, colorTex = 0;
    GLint  prevFBO = 0, prevViewport[4] = {};
    if (!beginFBO(kThumbSize, fbo, colorTex, prevFBO, prevViewport)) {
        sv->doneCurrent();
        return {};
    }

    // Compute aspect ratio from the material's first texture uniform so the
    // preview isn't stretched when the texture isn't square.
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

    const glm::mat4 model = glm::scale(glm::mat4(1.f), glm::vec3(sx, sy, 1.f));
    const glm::mat4 ident = glm::mat4(1.f);

    const auto& activeUniforms = shader->GetActiveUniforms();
    auto hasUniform = [&](const char* name) {
        return activeUniforms.find(name) != activeUniforms.end();
    };

    // Apply material uniforms first (colors, textures, user values), then
    // override the render-system uniforms so they can't be stomped.
    mat->ApplyUniforms();

    if (hasUniform("uModel"))  shader->SetMat4("uModel",  model);
    if (hasUniform("uView"))   shader->SetMat4("uView",   ident);
    if (hasUniform("uProj"))   shader->SetMat4("uProj",   ident);
    if (hasUniform("uSize"))   shader->SetVec2("uSize",   glm::vec2(1.f, 1.f));
    if (hasUniform("uPivot"))  shader->SetVec2("uPivot",  glm::vec2(0.f, 0.f));

    glad_glBindVertexArray(m_quadVAO);
    glad_glDrawArrays(GL_TRIANGLES, 0, 6);
    glad_glBindVertexArray(0);
    glad_glUseProgram(0);

    QPixmap result = readFBO(kThumbSize, fbo, colorTex, prevFBO, prevViewport);
    sv->doneCurrent();
    return result;
}
