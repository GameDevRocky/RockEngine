#include "engine/rendering/passes/ParticleSimulationPass.hpp"
#include "engine/rendering/core/ParticleManager.hpp"
#include "engine/components/ParticleComponent.hpp"
#include "engine/components/Transform.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/core/Scene.hpp"
#include "engine/core/TimeManager.hpp"
#include "engine/debug/FrameProfiler.hpp"
#include "Engine.hpp"

#include <glm/glm.hpp>

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

    for (auto* obj : scene->GetAllGameObjects()) {
        if (!obj || !obj->GetActive()) continue;
        ParticleComponent* emitter = obj->GetComponent<ParticleComponent>();
        if (!emitter || !emitter->GetEnabled()) continue;
        Transform* transform = obj->GetComponent<Transform>();
        if (!transform) continue;

        ParticleManager::Get().Simulate(emitter, transform->GetWorldPosition(),
                                       transform->GetWorldRotation(), dt, frameId);
    }

    // GC once per frame regardless of view/scene count.
    if (frameId != lastGcFrame) {
        ParticleManager::Get().GarbageCollect(frameId);
        lastGcFrame = frameId;
    }
}

void ParticleSimulationPass::Shutdown()
{
    // Nothing owned: programs/VBO/SSBOs all belong to ParticleManager, and this
    // pass creates no GL objects of its own.
}
