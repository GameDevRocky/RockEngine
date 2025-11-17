#pragma once

#include <vector>
#include <stdexcept>

#include "engine/rendering/passes/RenderPass.hpp"
#include "engine/core/System.hpp"
#include "engine/rendering/cameras/RenderCamera.hpp"
#include "engine/core/Scene.hpp"

class RenderPipeline : public System
{
public:
    RenderPipeline();
    ~RenderPipeline();

    void AddPass(RenderPass* pass);

    void Init() override;
    void Resize(int width, int height);
    void Render(RenderCamera& camera, Scene& scene);
    void Shutdown() override;

    // For SceneView / GameView to display the rendered result
    unsigned int GetOutputTexture() const { return outputTexture; }

private:
    void CreateOutputFBO(int width, int height);
    void DestroyOutputFBO();

private:
    std::vector<RenderPass*> passes;

    // Pipeline-owned output FBO
    unsigned int outputFBO = 0;
    unsigned int outputTexture = 0;
    unsigned int outputRBO = 0;

    int viewportWidth  = 1;
    int viewportHeight = 1;
};
