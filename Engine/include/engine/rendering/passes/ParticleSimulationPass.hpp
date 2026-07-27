#pragma once
#include <cstdint>
#include "engine/rendering/passes/RenderPass.hpp"

// Advances every ParticleComponent in the scene on the GPU, and garbage-collects
// emitter state. It draws NOTHING -- particle *drawing* lives in ScenePass, which
// interleaves emitters with sprites by (sorting layer, order in layer) so a
// particle system can sit behind or between sprite layers. A separate pass can
// only ever composite wholly over or under ScenePass, which is why the draw moved
// out; this pass must therefore be registered BEFORE ScenePass.
//
// Simulation is kept here rather than folded into ScenePass so it stays
// unconditional: emitters advance even when the camera's culling mask hides their
// layer, so an emitter isn't frozen by the view that happens to be rendering.
// ParticleManager owns the GPU state and guards per-frame work by frameId, so
// running across several viewports is safe. No VAO here -- nothing is drawn.
class ParticleSimulationPass : public RenderPass
{
public:
    ParticleSimulationPass() = default;

    void Execute(RenderCamera* camera, Scene* scene) override;
    void Shutdown() override;

private:
    std::uint64_t lastGcFrame = ~0ull;    // run GarbageCollect once per frame
};
