#pragma once

#include <vector>
#include <stdexcept>

#include "engine/rendering/passes/RenderPass.hpp"
#include "engine/core/System.hpp"
#include "engine/rendering/cameras/RenderCamera.hpp"
#include "engine/core/Scene.hpp"
#include "engine/rendering/passes/ClearPass.hpp"

class RenderPipeline : public System
{
public:
    RenderPipeline();
    ~RenderPipeline();

    void AddSetupPass(RenderPass* pass);
    void AddScenePass(RenderPass* pass);
    void AddFinalizePass(RenderPass* pass);
    void Init() override;
    void Resize(int width, int height);
    void Render(RenderCamera* camera, std::vector<Scene*> scenes);
    void Shutdown() override;

    unsigned int GetOutputTexture() const { return outputTexture; }

private:
    void CreateOutputFBO(int width, int height);
    void DestroyOutputFBO();

    std::vector<RenderPass*> setupPasses;
    std::vector<RenderPass*> scenePasses;
    std::vector<RenderPass*> finalizePasses;


    unsigned int outputFBO = 0;
    unsigned int outputTexture = 0;
    unsigned int outputRBO = 0;

    int viewportWidth  = 1;
    int viewportHeight = 1;
};
