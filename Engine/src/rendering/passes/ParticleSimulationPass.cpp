#include "engine/rendering/passes/ParticleSimulationPass.hpp"
#include "engine/rendering/core/ParticleManager.hpp"
#include "engine/components/ParticleComponent.hpp"
#include "engine/components/Transform.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/core/Scene.hpp"
#include "engine/core/SceneManager.hpp"
#include "engine/core/TimeManager.hpp"
#include "engine/debug/FrameProfiler.hpp"
#include "Engine.hpp"

#include <glm/glm.hpp>
#include <vector>

void ParticleSimulationPass::Execute(RenderCamera* camera, Scene* scene)
{
    ROCK_PROFILE_SCOPE("ParticleSimulationPass");
    if (!scene) return;

    // dt / frame index from the active container's TimeManager. In editor mode
    // this is the editor container (live preview); in play mode, the runtime one.
    Container* active = Engine::Get()->GetActiveContainer();
    TimeManager* time = active ? active->FindSystem<TimeManager>() : nullptr;
    if (!time) return;
    const float dt = time->DeltaTime();
    const std::uint64_t frameId = time->FrameCount();

    ParticleManager& particles = ParticleManager::Get();
    if (!particles.BeginSimulationFrame(dt, frameId)) return;

    // RenderPipeline invokes scene passes once per loaded scene. The first
    // invocation collects every scene so the global dispatch is complete;
    // BeginSimulationFrame suppresses all later scene and viewport invocations.
    SceneManager* sceneManager = active->FindSystem<SceneManager>();
    const std::vector<Scene*> scenes = sceneManager
        ? sceneManager->GetScenes()
        : std::vector<Scene*>{ scene };
    for (Scene* simulationScene : scenes) {
        if (!simulationScene) continue;
        for (auto* obj : simulationScene->GetAllGameObjects()) {
            if (!obj || !obj->GetActive()) continue;
            ParticleComponent* emitter = obj->GetComponent<ParticleComponent>();
            if (!emitter || !emitter->GetEnabled()) continue;
            Transform* transform = obj->GetComponent<Transform>();
            if (!transform) continue;

            particles.QueueEmitter(emitter, transform->GetWorldPosition(),
                                   transform->GetWorldRotation());
        }
    }

    particles.EndSimulationFrame();

    // GC once per frame regardless of view/scene count.
    if (frameId != lastGcFrame) {
        particles.GarbageCollect(frameId);
        lastGcFrame = frameId;
    }
}

void ParticleSimulationPass::Shutdown()
{
    // Nothing owned: programs/VBO/SSBOs all belong to ParticleManager, and this
    // pass creates no GL objects of its own.
}
