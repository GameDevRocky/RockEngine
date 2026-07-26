#include "engine/rendering/passes/ParticlePass.hpp"
#include "engine/rendering/core/ParticleManager.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/rendering/core/Sprite.hpp"
#include "engine/rendering/core/Texture2D.hpp"
#include "engine/rendering/cameras/RenderCamera.hpp"
#include "engine/components/ParticleComponent.hpp"
#include "engine/components/Transform.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/core/Scene.hpp"
#include "engine/core/TimeManager.hpp"
#include "engine/debug/FrameProfiler.hpp"
#include "Engine.hpp"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <algorithm>
#include <vector>

void ParticlePass::Init()
{
    ParticleManager& pm = ParticleManager::Get();
    pm.EnsureInitialized();

    // Build this pass's own VAO over the manager's shared quad VBO. The VAO is
    // per-context (not shared); the VBO it references is.
    glad_glGenVertexArrays(1, &vao);
    glad_glBindVertexArray(vao);
    glad_glBindBuffer(GL_ARRAY_BUFFER, pm.GetQuadVBO());
    glad_glEnableVertexAttribArray(0);
    glad_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glad_glEnableVertexAttribArray(1);
    glad_glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glad_glBindVertexArray(0);
    glad_glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void ParticlePass::Execute(RenderCamera* camera, Scene* scene)
{
    ROCK_PROFILE_SCOPE("ParticlePass");
    if (!scene || !camera) return;

    // dt / frame index from the active container's TimeManager. In editor mode
    // this is the editor container (live preview); in play mode, the runtime one.
    Container* active = Engine::Get()->GetActiveContainer();
    TimeManager* time = active ? active->FindSystem<TimeManager>() : nullptr;
    if (!time) return;
    const float dt = time->DeltaTime();
    const std::uint64_t frameId = time->FrameCount();

    // Collect enabled emitters, sorted by sortingOrder (particles have their own
    // ordering; they don't interleave with sprite sorting layers).
    struct Item { ParticleComponent* emitter; Transform* transform; int order; };
    std::vector<Item> items;
    for (auto* obj : scene->GetAllGameObjects()) {
        if (!obj || !obj->GetActive()) continue;
        ParticleComponent* emitter = obj->GetComponent<ParticleComponent>();
        if (!emitter || !emitter->GetEnabled()) continue;
        Transform* transform = obj->GetComponent<Transform>();
        if (!transform) continue;
        items.push_back({ emitter, transform, emitter->GetSortingOrder() });
    }

    if (!items.empty()) {
        std::sort(items.begin(), items.end(),
                  [](const Item& a, const Item& b) { return a.order < b.order; });

        ParticleManager& pm = ParticleManager::Get();
        const glm::mat4& view = camera->GetViewMatrix();
        const glm::mat4& proj = camera->GetProjectionMatrix();

        glad_glEnable(GL_BLEND);
        glad_glDepthMask(GL_FALSE);   // particles don't occlude; write color only

        for (const Item& it : items) {
            ParticleComponent* e = it.emitter;
            const glm::vec2 worldPos = it.transform->GetWorldPosition();
            const float worldRot = it.transform->GetWorldRotation();

            pm.Simulate(e, worldPos, worldRot, dt, frameId);

            // Blend mode per emitter.
            if (e->GetBlendMode() == ParticleComponent::BlendMode::Additive)
                glad_glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            else
                glad_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            // Local-space emitters draw through the emitter world matrix; World-
            // space particles are already baked into world coords (identity).
            glm::mat4 model(1.0f);
            if (e->GetSpace() == ParticleComponent::SimulationSpace::Local)
                model = it.transform->GetWorldMatrix();

            unsigned int texId = 0;
            if (!e->GetSpriteID().empty()) {
                if (Sprite* sprite = AssetManager::Get().GetSprite(e->GetSpriteID()))
                    if (Texture2D* tex = sprite->GetTexture())
                        texId = tex->GetTextureID();
            }

            pm.Draw(e, view, proj, model, texId, vao);
        }

        glad_glDepthMask(GL_TRUE);
        glad_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);   // restore default
    }

    // GC once per frame regardless of view/scene count.
    if (frameId != lastGcFrame) {
        ParticleManager::Get().GarbageCollect(frameId);
        lastGcFrame = frameId;
    }
}

void ParticlePass::Shutdown()
{
    // Only the pass-owned VAO. Programs/VBO/SSBOs belong to ParticleManager.
    if (vao) glad_glDeleteVertexArrays(1, &vao);
    vao = 0;
}
