#include "engine/rendering/passes/PickingPass.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include "engine/components/Transform.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/rendering/core/Material.hpp"
#include "engine/core/Scene.hpp"
#include "engine/core/SceneManager.hpp"
#include "engine/debug/Console.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "Engine.hpp"
#include "engine/debug/FrameProfiler.hpp"
#include <algorithm>
#include <vector>
#include <iostream>

void PickingPass::Init()
{
    glad_glGenFramebuffers(1, &fbo);
    glad_glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glad_glGenTextures(1, &pickingTexture);
    glad_glBindTexture(GL_TEXTURE_2D, pickingTexture);
    glad_glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, 1, 1, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glad_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pickingTexture, 0);

    glad_glGenTextures(1, &depthTexture);
    glad_glBindTexture(GL_TEXTURE_2D, depthTexture);
    glad_glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 1, 1, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glad_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);

    if (glad_glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "Picking FBO not complete!" << std::endl;

    glad_glBindFramebuffer(GL_FRAMEBUFFER, 0);
    float quadVerts[] = {
        -0.5f, -0.5f, 0.0f, 0.0f,
         0.5f, -0.5f, 1.0f, 0.0f,
         0.5f,  0.5f, 1.0f, 1.0f,

        -0.5f, -0.5f, 0.0f, 0.0f,
         0.5f,  0.5f, 1.0f, 1.0f,
        -0.5f,  0.5f, 0.0f, 1.0f
    };

    glad_glGenVertexArrays(1, &vao);
    glad_glGenBuffers(1, &vbo);
    glad_glBindVertexArray(vao);
    glad_glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glad_glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    glad_glEnableVertexAttribArray(0); 
    glad_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glad_glEnableVertexAttribArray(1); 
    glad_glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glad_glBindVertexArray(0);

    // Create fullscreen quad for debug overlay
    float fullscreenQuad[] = {
        // pos        // uv
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f,

        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 1.0f
    };

    glad_glGenVertexArrays(1, &debugVao);
    glad_glGenBuffers(1, &debugVbo);
    glad_glBindVertexArray(debugVao);
    glad_glBindBuffer(GL_ARRAY_BUFFER, debugVbo);
    glad_glBufferData(GL_ARRAY_BUFFER, sizeof(fullscreenQuad), fullscreenQuad, GL_STATIC_DRAW);
    glad_glEnableVertexAttribArray(0);
    glad_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glad_glEnableVertexAttribArray(1);
    glad_glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glad_glBindVertexArray(0);

    const char* debugVert = R"(#version 460 core
    layout(location = 0) in vec2 aPos;
    layout(location = 1) in vec2 aTexCoord;
    out vec2 vTexCoord;
    void main() {
        vTexCoord = aTexCoord;
        gl_Position = vec4(aPos, 0.0, 1.0);
    })";

    const char* debugFrag = R"(#version 460 core
    in vec2 vTexCoord;
    out vec4 FragColor;
    uniform usampler2D uPickingTexture;
    uniform float uAlpha;
    void main() {
        uint id = texture(uPickingTexture, vTexCoord).r;
        if (id == 0u) discard;
        // Convert ID to color using simple hash
        float r = fract(float(id) * 0.123);
        float g = fract(float(id) * 0.456);
        float b = fract(float(id) * 0.789);
        FragColor = vec4(r, g, b, uAlpha);
    })";

    GLuint vs = glad_glCreateShader(GL_VERTEX_SHADER);
    GLuint fs = glad_glCreateShader(GL_FRAGMENT_SHADER);
    glad_glShaderSource(vs, 1, &debugVert, nullptr);
    glad_glCompileShader(vs);
    glad_glShaderSource(fs, 1, &debugFrag, nullptr);
    glad_glCompileShader(fs);

    debugShaderProgram = glad_glCreateProgram();
    glad_glAttachShader(debugShaderProgram, vs);
    glad_glAttachShader(debugShaderProgram, fs);
    glad_glLinkProgram(debugShaderProgram);
    glad_glDeleteShader(vs);
    glad_glDeleteShader(fs);

    shader = AssetManager::Get().GetShaderByName("picking");
}

void PickingPass::Resize(int width, int height)
{
    if (width <= 0 || height <= 0) return;
    viewportWidth = width;
    viewportHeight = height;

    glad_glBindTexture(GL_TEXTURE_2D, pickingTexture);
    glad_glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, width, height, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);

    glad_glBindTexture(GL_TEXTURE_2D, depthTexture);
    glad_glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
}

void PickingPass::Execute(RenderCamera* /*camera*/, Scene* /*scene*/)
{
    // Deliberately empty. Re-rendering the whole scene into the pick buffer every
    // frame just to service the occasional mouse click doubled the editor's scene
    // draw cost. Picking now renders on demand via RenderPickBuffer(), called from
    // EditorRenderView::Pick() right before ReadPixel.
}

void PickingPass::RenderPickBuffer(RenderCamera* camera)
{
    ROCK_PROFILE_SCOPE("PickingPass");
    if (!camera) return;
    pickIdToObjectId.clear();
    SceneManager* sceneManager = Engine::Get()->GetActiveContainer()->FindSystem<SceneManager>();
    if (!sceneManager) return;
    const auto& scenes = sceneManager->GetScenes();
    GLint prevFBO = 0;
    glad_glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
    glad_glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    GLenum status = glad_glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        glad_glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
        return;
    }
    glad_glViewport(0, 0, viewportWidth, viewportHeight);

    uint32_t clearColor = 0;
    glad_glClearBufferuiv(GL_COLOR, 0, &clearColor);
    glad_glClear(GL_DEPTH_BUFFER_BIT);

    // Save what we're about to stomp. RenderPickBuffer runs on demand from
    // Pick(), OUTSIDE the pipeline, so anything left changed here leaks into the
    // next frame's rendering -- ScenePass enables GL_BLEND once in Init() and
    // never re-enables it per frame, so leaving blend off here silently killed
    // sprite alpha blending until something else happened to turn it back on.
    const GLboolean prevBlend     = glad_glIsEnabled(GL_BLEND);
    const GLboolean prevDepthTest = glad_glIsEnabled(GL_DEPTH_TEST);

    glad_glDisable(GL_BLEND);
    // Depth testing is actively WRONG here. Transform::localPosition is a vec2,
    // so every sprite sits at z=0; with the default GL_LESS an equal-depth
    // fragment FAILS, which means the first sprite drawn to a pixel wins and
    // every sprite behind it in draw order is rejected. Picking then resolved to
    // whatever came first in the hierarchy rather than what the user sees.
    //
    // Painter's algorithm instead: no depth test, last write wins, drawn in
    // ScenePass's exact order -- so the pick buffer's final value at a pixel is
    // by construction the same sprite that ended up visible there.
    glad_glDisable(GL_DEPTH_TEST);

    shader->Bind();
    shader->SetMat4("uView", camera->GetViewMatrix());
    shader->SetMat4("uProj", camera->GetProjectionMatrix());

    glad_glBindVertexArray(vao);

    uint32_t pickId = 1;

    // One entry per pickable sprite, sorted per scene exactly like ScenePass.
    struct PickDraw {
        GameObject*     obj           = nullptr;
        SpriteRenderer* renderer      = nullptr;
        Transform*      transform     = nullptr;
        int             layerPriority = 0;
        int             sortingOrder  = 0;
        GLuint          shaderId      = 0;
    };
    std::vector<PickDraw> draws;

    // Scenes stay an OUTER loop and are sorted within, not across: the pipeline
    // runs ScenePass once per scene, so a later scene always paints over an
    // earlier one regardless of layer. Sorting globally would break that.
    for (Scene* currentScene : scenes) {
        if (!currentScene) continue;

        draws.clear();
        for (auto* obj : currentScene->GetAllGameObjects()) {
            if (!obj || !obj->GetActive()) continue;

            SpriteRenderer* spr = obj->GetComponent<SpriteRenderer>();
            // Match ScenePass's visibility test exactly, including the camera's
            // culling mask -- a sprite on a layer this camera doesn't render must
            // not be clickable either.
            if (!spr || !spr->GetEnabled() || !spr->GetVisible()) continue;
            if (camera && !camera->PassesCullingMask(spr->GetSortingLayer())) continue;

            Transform* transform = obj->GetComponent<Transform>();
            if (!transform) continue;

            PickDraw d;
            d.obj           = obj;
            d.renderer      = spr;
            d.transform     = transform;
            d.layerPriority = layerManager ? layerManager->GetPriority(spr->GetSortingLayer()) : 0;
            d.sortingOrder  = spr->GetSortingOrder();
            if (Material* mat = spr->GetMaterial())
                if (Shader* s = mat->GetShader()) d.shaderId = s->GetProgramID();
            draws.push_back(d);
        }

        // Same keys as ScenePass, and stable for the same reason: on a full tie
        // both passes then fall back to scene scan order, which is identical --
        // so the two can never disagree about which sprite is on top.
        std::stable_sort(draws.begin(), draws.end(), [](const PickDraw& a, const PickDraw& b) {
            if (a.layerPriority != b.layerPriority) return a.layerPriority < b.layerPriority;
            if (a.sortingOrder  != b.sortingOrder)  return a.sortingOrder  < b.sortingOrder;
            return a.shaderId < b.shaderId;
        });

        for (const PickDraw& d : draws) {
            GameObject* obj = d.obj;
            SpriteRenderer* spr = d.renderer;
            Transform* transform = d.transform;
            pickIdToObjectId[pickId] = obj->GetID();
            shader->SetInt("uId", pickId);
            shader->SetMat4("uModel", transform->GetWorldMatrix());
            Sprite* sprite = spr->GetSprite();
            glActiveTexture(GL_TEXTURE0);
            if(sprite && sprite->GetTexture()) {
                glad_glBindTexture(GL_TEXTURE_2D, sprite->GetTexture()->GetTextureID());
            } else {
                 glad_glBindTexture(GL_TEXTURE_2D, 0);
            }
            shader->SetInt("uTexture", 0);
            if(sprite) {
                 glm::vec2 uvScale = sprite->GetUVMax() - sprite->GetUVMin();
                 glm::vec2 uvOffset = sprite->GetUVMin();
                 if (spr->GetFlipX()) { uvScale.x *= -1.0f; uvOffset.x = sprite->GetUVMax().x; }
                 if (spr->GetFlipY()) { uvScale.y *= -1.0f; uvOffset.y = sprite->GetUVMax().y; }
                 shader->SetVec2("uUVScale", uvScale);
                 shader->SetVec2("uUVOffset", uvOffset);
                 glm::vec2 worldSize = EngineUtils::RenderUtils::PixelsToWorld(sprite->GetPixelSize());
                 shader->SetVec2("uSize", worldSize);
                 shader->SetVec2("uPivot", sprite->GetPivot());
            } else {
                 shader->SetVec2("uUVScale", glm::vec2(1.0f));
                 shader->SetVec2("uUVOffset", glm::vec2(0.0f));
                 shader->SetVec2("uSize", glm::vec2(1.0f));
                 shader->SetVec2("uPivot", glm::vec2(0.5f));
            }

            glad_glDrawArrays(GL_TRIANGLES, 0, 6);
            
            pickId++;
        }
    }

    glad_glBindVertexArray(0);
    glad_glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
    glad_glViewport(0, 0, viewportWidth, viewportHeight);

    // Hand the context back exactly as we found it (see the note at the top).
    if (prevBlend)     glad_glEnable(GL_BLEND);     else glad_glDisable(GL_BLEND);
    if (prevDepthTest) glad_glEnable(GL_DEPTH_TEST); else glad_glDisable(GL_DEPTH_TEST);

    if (debugDraw) {
        DrawDebugOverlay();
    }
}

uint32_t PickingPass::ReadPixel(int x, int y)
{
        if (x < 0 || x >= viewportWidth || y < 0 || y >= viewportHeight) {
        std::cerr << "  ERROR: Coordinates out of bounds!" << std::endl;
        return 0;
    }
    
    glad_glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glad_glReadBuffer(GL_COLOR_ATTACHMENT0);
    
    uint32_t pixel = 0;
    
    glad_glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, &pixel);    
    glad_glReadBuffer(GL_NONE);
    glad_glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    return pixel;
}

void PickingPass::Shutdown()
{
    glad_glDeleteFramebuffers(1, &fbo);
    glad_glDeleteTextures(1, &pickingTexture);
    glad_glDeleteTextures(1, &depthTexture);
    glad_glDeleteVertexArrays(1, &vao);
    glad_glDeleteBuffers(1, &vbo);
    glad_glDeleteVertexArrays(1, &debugVao);
    glad_glDeleteBuffers(1, &debugVbo);
    if (debugShaderProgram) {
        glad_glDeleteProgram(debugShaderProgram);
        debugShaderProgram = 0;
    }
}

void PickingPass::DrawDebugOverlay()
{
    glad_glDisable(GL_DEPTH_TEST);
    glad_glEnable(GL_BLEND);
    glad_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glad_glUseProgram(debugShaderProgram);
    glad_glUniform1i(glad_glGetUniformLocation(debugShaderProgram, "uPickingTexture"), 0);
    glad_glUniform1f(glad_glGetUniformLocation(debugShaderProgram, "uAlpha"), 0.5f);
    
    glad_glActiveTexture(GL_TEXTURE0);
    glad_glBindTexture(GL_TEXTURE_2D, pickingTexture);
    
    glad_glBindVertexArray(debugVao);
    glad_glDrawArrays(GL_TRIANGLES, 0, 6);
    glad_glBindVertexArray(0);
    
    glad_glUseProgram(0);
    glad_glEnable(GL_DEPTH_TEST);
}

std::string PickingPass::GetPickedObjectId(uint32_t pickId) const
{
    auto it = pickIdToObjectId.find(pickId);
    return (it != pickIdToObjectId.end()) ? it->second : "";
}

