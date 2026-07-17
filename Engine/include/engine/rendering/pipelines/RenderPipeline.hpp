#pragma once

#include <vector>
#include <stdexcept>

#include "engine/rendering/passes/RenderPass.hpp"
#include "engine/rendering/cameras/RenderCamera.hpp"
#include "engine/rendering/core/RenderTarget.hpp"
#include "engine/core/Scene.hpp"
#include "engine/rendering/passes/ClearPass.hpp"

// A plain, non-Container-owned class: render resources have no per-world
// identity (the editor and runtime containers share one screen and one GL
// context), so a pipeline is constructed and owned directly by a viewport,
// not registered as a System.
class RenderPipeline
{
public:
    RenderPipeline() = default;
    ~RenderPipeline();

    void AddSetupPass(RenderPass* pass);
    void AddScenePass(RenderPass* pass);
    void AddFinalizePass(RenderPass* pass);
    void Init();
    void Resize(int width, int height);
    void Render(RenderCamera* camera, const std::vector<Scene*>& scenes);
    void Shutdown();   // idempotent; safe to call more than once (dtor calls it too)

    unsigned int GetOutputTexture() const { return outputTarget.GetColorTexture(); }

private:
    std::vector<RenderPass*> setupPasses;
    std::vector<RenderPass*> scenePasses;
    std::vector<RenderPass*> finalizePasses;

    RenderTarget outputTarget;
    bool shutdown = false;
};
