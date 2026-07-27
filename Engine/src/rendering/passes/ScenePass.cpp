#include "engine/rendering/passes/ScenePass.hpp"
#include "engine/rendering/core/ParticleManager.hpp"
#include "engine/rendering/core/Sprite.hpp"
#include "engine/rendering/core/Texture2D.hpp"
#include "engine/components/ParticleComponent.hpp"
#include "engine/debug/FrameProfiler.hpp"
#include <algorithm>
#include <vector>

void ScenePass::Init(){
    float quadVerts[] =
    {
        // pos      // uv
        -0.5f, -0.5f, 0.0f, 0.0f,
            0.5f, -0.5f, 1.0f, 0.0f,
            0.5f,  0.5f, 1.0f, 1.0f,

        -0.5f, -0.5f, 0.0f, 0.0f,
            0.5f,  0.5f, 1.0f, 1.0f,
        -0.5f,  0.5f, 0.0f, 1.0f
    };
    glad_glEnable(GL_BLEND);
    glad_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


    glad_glGenVertexArrays(1, &vao);
    glad_glGenBuffers(1, &vbo);

    glad_glBindVertexArray(vao);
    glad_glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glad_glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);

    glad_glEnableVertexAttribArray(0); // pos
    glad_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    glad_glEnableVertexAttribArray(1); // uv
    glad_glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glad_glBindVertexArray(0);

    // Particle draws go through this pass too (see the class comment), so make
    // sure the manager's programs + shared quad exist and build a VAO over them.
    ParticleManager& pm = ParticleManager::Get();
    pm.EnsureInitialized();

    glad_glGenVertexArrays(1, &particleVao);
    glad_glBindVertexArray(particleVao);
    glad_glBindBuffer(GL_ARRAY_BUFFER, pm.GetQuadVBO());
    glad_glEnableVertexAttribArray(0);
    glad_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glad_glEnableVertexAttribArray(1);
    glad_glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glad_glBindVertexArray(0);
    glad_glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void ScenePass::Execute(RenderCamera* camera, Scene* scene)
{
    ROCK_PROFILE_SCOPE("ScenePass");
    if (!scene) return;
    const auto& objects = scene->GetAllGameObjects();

    // One entry per renderable. Exactly one of `renderer` / `emitter` is set --
    // sprites and particle emitters sort against each other in a single list, so
    // an emitter's layer/order places it among the sprites rather than on top.
    struct DrawCall
    {
        Transform*         transform      = nullptr;
        SpriteRenderer*    renderer       = nullptr;   // sprite path
        Material*          mat            = nullptr;
        Shader*            shader         = nullptr;
        ParticleComponent* emitter        = nullptr;   // particle path
        int                layerPriority  = 0;
        int                sortingOrder   = 0;
    };

    std::vector<DrawCall> drawCalls;
    drawCalls.reserve(objects.size());

    for (auto* obj : objects)
    {
        if (!obj || !obj->GetActive()) continue;

        Transform* transform = obj->GetComponent<Transform>();
        if (!transform)
        {
            // Only worth warning about for something that wanted to draw.
            if ((obj->GetComponent<SpriteRenderer>() || obj->GetComponent<ParticleComponent>())
                && warnedObjects.insert(obj).second)
                Console::Alert("No Loaded Transform");
            continue;
        }

        if (SpriteRenderer* renderer = obj->GetComponent<SpriteRenderer>())
        {
            if (renderer->GetEnabled() && renderer->GetVisible()
                && (!camera || camera->PassesCullingMask(renderer->GetSortingLayer())))
            {
                Material* mat = renderer->GetMaterial();
                if (!mat)
                {
                    if (!defaultMaterial)
                        defaultMaterial = AssetManager::Get().GetMaterialByName("default");
                    mat = defaultMaterial;
                    if (warnedObjects.insert(obj).second)
                        Console::Alert("Assigning Default Material to " + transform->GetGameObject()->GetName());
                }
                if (mat && mat->GetShader())
                {
                    DrawCall dc;
                    dc.transform = transform;
                    dc.renderer  = renderer;
                    dc.mat       = mat;
                    dc.shader    = mat->GetShader();
                    dc.layerPriority = layerManager ? layerManager->GetPriority(renderer->GetSortingLayer()) : 0;
                    dc.sortingOrder  = renderer->GetSortingOrder();
                    drawCalls.push_back(dc);
                }
            }
        }

        // An object can carry both a sprite and an emitter; each is its own entry
        // and sorts on its own layer/order.
        if (ParticleComponent* emitter = obj->GetComponent<ParticleComponent>())
        {
            if (emitter->GetEnabled()
                && (!camera || camera->PassesCullingMask(emitter->GetSortingLayer())))
            {
                DrawCall dc;
                dc.transform = transform;
                dc.emitter   = emitter;
                dc.layerPriority = layerManager ? layerManager->GetPriority(emitter->GetSortingLayer()) : 0;
                dc.sortingOrder  = emitter->GetSortingOrder();
                drawCalls.push_back(dc);
            }
        }
    }

    std::sort(drawCalls.begin(), drawCalls.end(), [](const DrawCall& a, const DrawCall& b)
    {
        if (a.layerPriority != b.layerPriority) return a.layerPriority < b.layerPriority;
        if (a.sortingOrder  != b.sortingOrder)  return a.sortingOrder  < b.sortingOrder;
        // Within a layer+order tie, group sprites by shader to cut rebinds, and
        // keep sprites ahead of particles so emitters composite over the sprites
        // they tie with (particles are usually the effect layered on top).
        if ((a.emitter != nullptr) != (b.emitter != nullptr)) return b.emitter != nullptr;
        if (a.shader && b.shader) return a.shader->GetProgramID() < b.shader->GetProgramID();
        return false;
    });

    const glm::mat4& viewMatrix = camera->GetViewMatrix();
    const glm::mat4& projMatrix = camera->GetProjectionMatrix();

    ParticleManager& pm = ParticleManager::Get();
    glad_glBindVertexArray(vao);
    GLuint lastProgramID = 0;
    bool particleStateActive = false;   // depth-mask/blend left in particle config

    for (const DrawCall& dc : drawCalls)
    {
        if (dc.emitter)
        {
            // Particles don't occlude, and each emitter picks its own blend mode.
            if (!particleStateActive)
            {
                glad_glDepthMask(GL_FALSE);
                particleStateActive = true;
            }
            if (dc.emitter->GetBlendMode() == ParticleComponent::BlendMode::Additive)
                glad_glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            else
                glad_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            // Local-space emitters draw through the emitter world matrix; World-
            // space particles already carry world coords (identity).
            glm::mat4 model(1.0f);
            if (dc.emitter->GetSpace() == ParticleComponent::SimulationSpace::Local)
                model = dc.transform->GetWorldMatrix();

            // Resolve the sprite to a texture AND its UV sub-rect: a sprite is a
            // region of a (possibly atlas) texture, so sampling the raw texture
            // over [0,1] would show the whole sheet instead of the sprite. Flip is
            // folded into the scale's sign about the sprite's own rect, so a
            // flipped atlas sprite stays inside its region -- same convention as
            // SpriteRenderer::OverrideUniforms.
            unsigned int texId = 0;
            glm::vec2 uvScale(1.0f, 1.0f);
            glm::vec2 uvOffset(0.0f, 0.0f);
            if (!dc.emitter->GetSpriteID().empty()) {
                if (Sprite* sprite = AssetManager::Get().GetSprite(dc.emitter->GetSpriteID())) {
                    if (Texture2D* tex = sprite->GetTexture()) {
                        texId = tex->GetTextureID();
                        uvScale  = sprite->GetUVMax() - sprite->GetUVMin();
                        uvOffset = sprite->GetUVMin();
                        if (dc.emitter->GetFlipX()) {
                            uvScale.x  = -uvScale.x;
                            uvOffset.x = sprite->GetUVMax().x;
                        }
                        if (dc.emitter->GetFlipY()) {
                            uvScale.y  = -uvScale.y;
                            uvOffset.y = sprite->GetUVMax().y;
                        }
                    }
                }
            }

            // Draw() owns its program and unbinds the VAO it was handed, so the
            // sprite path must re-bind both afterwards.
            pm.Draw(dc.emitter, viewMatrix, projMatrix, model, texId,
                    uvScale, uvOffset, particleVao);
            glad_glBindVertexArray(vao);
            lastProgramID = 0;
            continue;
        }

        if (particleStateActive)
        {
            glad_glDepthMask(GL_TRUE);
            glad_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            particleStateActive = false;
        }

        if (dc.shader->GetProgramID() != lastProgramID)
        {
            dc.shader->Bind();
            dc.shader->SetMat4("uView", viewMatrix);
            dc.shader->SetMat4("uProj", projMatrix);
            lastProgramID = dc.shader->GetProgramID();
        }

        dc.shader->SetMat4("uModel", dc.transform->GetWorldMatrix());
        dc.mat->ApplyUniforms();
        dc.renderer->OverrideUniforms();

        glad_glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    if (particleStateActive)
    {
        glad_glDepthMask(GL_TRUE);
        glad_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    glad_glBindVertexArray(0);
    glad_glUseProgram(0);
}

void ScenePass::Shutdown()
{
    if (vbo) glad_glDeleteBuffers(1, &vbo);
    if (vao) glad_glDeleteVertexArrays(1, &vao);
    // particleVao is ours (VAOs are per-context); the VBO it references belongs
    // to ParticleManager and must not be deleted here.
    if (particleVao) glad_glDeleteVertexArrays(1, &particleVao);
    vao = vbo = particleVao = 0;
}
