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

#include <any>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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

    // A thumbnail shows the ASSET, not the asset lit by whatever happens to be in
    // the scene. uLitAmount 0 collapses the shader's light term to 1.0, which is
    // also what keeps this correct outside a LightingPass: the light UBO is bound
    // per-context and nothing bound it here.
    if (has("uLitAmount"))    shader->SetFloat("uLitAmount", 0.f);
    if (has("uHasNormalMap")) shader->SetFloat("uHasNormalMap", 0.f);

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

namespace {

// id -> rendered thumbnail. Every entry is a pure function of the asset's state
// at render time, so the eviction wiring below is the whole correctness story.
std::unordered_map<std::string, QPixmap> g_cache;

// Assets we already hold a change subscription on, so repeated cache misses for
// the same id don't stack duplicate callbacks on it.
std::unordered_set<std::string> g_watched;

// Wire the cache to AssetManager on first use rather than at startup: this is
// editor-side code with no init hook of its own, and the first thumbnail request
// is by definition before any thumbnail could go stale.
void EnsureManagerSubscribed()
{
    static bool subscribed = false;
    if (subscribed) return;
    subscribed = true;

    AssetManager& am = AssetManager::Get();

    // Payload is the id (see AssetManager::RemoveAsset / RemoveSprite).
    am.Subscribe([](std::any payload) {
        if (payload.type() == typeid(std::string)) {
            const auto id = std::any_cast<std::string>(payload);
            g_watched.erase(id);   // the Observable died with the asset
            Invalidate(id);
        } else {
            InvalidateAll();       // unknown payload: assume the worst
        }
        return true;
    }, AssetManager::ASSET_REMOVED_EVENT);

    // Reloading an asset in place (the file watcher re-reading a meta) keeps the
    // id and replaces the contents, which is exactly a stale-thumbnail case.
    am.Subscribe([](std::any payload) {
        if (payload.type() == typeid(Resource*)) {
            if (Resource* r = std::any_cast<Resource*>(payload)) {
                g_watched.erase(r->GetID());
                Invalidate(r->GetID());
            }
        }
        return true;
    }, AssetManager::ASSET_ADDED_EVENT);
}

// Subscribe once per asset so editing it drops the pictures drawn from it.
// ANY_EVENT deliberately: a thumbnail depends on very nearly every field a
// resource has, and a hand-maintained list of "the events that change the
// picture" would rot the first time someone adds a property.
void Watch(const std::vector<Resource*>& assets)
{
    for (Resource* r : assets) {
        if (!r) continue;
        const std::string id = r->GetID();
        if (!g_watched.insert(id).second) continue;
        // Captures the id by value, not the Resource*. The subscription is owned
        // by the resource and dies with it, but Invalidate has to look the
        // dependents up through AssetManager anyway, and an id stays meaningful
        // there where a pointer would not.
        r->Subscribe([id]() { Invalidate(id); return true; });
    }
}

// Cache a freshly rendered thumbnail and start watching what it was drawn from.
// A null pixmap is deliberately NOT cached: it means the render failed, almost
// always because there is no GL context yet, and storing that would pin an empty
// cell for the rest of the session.
QPixmap Store(const std::string& id, QPixmap px, const std::vector<Resource*>& sources)
{
    if (px.isNull()) return px;
    Watch(sources);
    g_cache[id] = px;
    return px;
}

} // namespace

// ─── Cache-fronted entry points ──────────────────────────────────────────────

QPixmap forMaterial(const std::string& id)
{
    EnsureManagerSubscribed();
    if (auto it = g_cache.find(id); it != g_cache.end()) return it->second;

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

    // Watch everything the picture was drawn from, not just the material: a
    // recoloured texture or a recompiled shader changes it just as much as an
    // edit to the material itself.
    std::vector<Resource*> sources{ mat, shader };
    for (const auto& [uname, texId] : mat->GetTexUniforms())
        if (Texture2D* t = AssetManager::Get().GetTexture(texId)) sources.push_back(t);

    return Store(id, result, sources);
}

QPixmap forSprite(const std::string& id)
{
    EnsureManagerSubscribed();
    if (auto it = g_cache.find(id); it != g_cache.end()) return it->second;

    Sprite* sprite = AssetManager::Get().GetSprite(id);
    if (!sprite) return {};
    QPixmap px = renderTextureQuad(kSize, sprite->GetTexture(), sprite->GetUVMin(),
                                   sprite->GetUVMax(), sprite->GetPivot());
    return Store(id, px, { sprite, sprite->GetTexture() });
}

QPixmap forTexture(const std::string& id)
{
    EnsureManagerSubscribed();
    if (auto it = g_cache.find(id); it != g_cache.end()) return it->second;

    Texture2D* tex = AssetManager::Get().GetTexture(id);
    QPixmap px = renderTextureQuad(kSize, tex, glm::vec2(0.f), glm::vec2(1.f), glm::vec2(0.f));
    return Store(id, px, { tex });
}

// ─── Invalidation ────────────────────────────────────────────────────────────

void Invalidate(const std::string& id)
{
    g_cache.erase(id);
    if (g_cache.empty()) return;

    // Thumbnails compose: a sprite's is drawn through its texture, a material's
    // through its shader and its texture uniforms. Dropping only the edited
    // asset's own entry would leave the picker showing pre-edit art for
    // everything drawn from it. Resolved against AssetManager on demand rather
    // than from a stored reverse index — the cache is a few hundred entries at
    // worst and this runs on a human-paced event, so an index would be more to
    // keep correct than it saves.
    AssetManager& am = AssetManager::Get();
    for (auto it = g_cache.begin(); it != g_cache.end(); ) {
        bool derived = false;

        if (Sprite* s = am.GetSprite(it->first)) {
            derived = (s->GetTextureID() == id);
        } else if (Material* m = am.GetMaterial(it->first)) {
            Shader* sh = m->GetShader();
            derived = (sh && sh->GetID() == id);
            if (!derived)
                for (const auto& [uname, texId] : m->GetTexUniforms())
                    if (texId == id) { derived = true; break; }
        }

        if (derived) it = g_cache.erase(it);
        else         ++it;
    }
}

void InvalidateAll()
{
    g_cache.clear();
    // Deliberately keeps g_watched: those subscriptions live on the assets and
    // are still valid, and re-adding them would double up on every asset the
    // cache refills with.
}

}