#pragma once
#include <cstdint>
#include "engine/rendering/passes/RenderPass.hpp"

// Draws every ParticleComponent in the scene, GPU-simulated. Simulation state
// and the compute/draw programs live in ParticleManager (shared across
// contexts); this pass owns only its per-context VAO (VAOs are NOT shared by
// AA_ShareOpenGLContexts) and drives the per-frame simulate + draw. Added as a
// scene pass after ScenePass so particles composite over sprites.
class ParticlePass : public RenderPass
{
public:
    ParticlePass() = default;

    void Init() override;
    void Execute(RenderCamera* camera, Scene* scene) override;
    void Shutdown() override;

private:
    unsigned int vao = 0;                 // per-context; references ParticleManager's shared VBO
    std::uint64_t lastGcFrame = ~0ull;    // run GarbageCollect once per frame
};
