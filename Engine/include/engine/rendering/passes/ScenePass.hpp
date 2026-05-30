#pragma once
#include <glad/glad.h>
#include "engine/rendering/passes/RenderPass.hpp"
#include "engine/components/Transform.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include "engine/rendering/core/Material.hpp"
#include "engine/rendering/core/SharedResources.hpp"
#include "engine/debug/Console.hpp"
#include "engine/core/LayerManager.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Engine.hpp"
#include "engine/utils/EngineUtils.hpp"

using namespace EngineUtils;

class ScenePass : public RenderPass
{ 
public:
    ScenePass() = default;

    void Init() override;

    void Resize(int width, int height) override ;

    void Execute(RenderCamera* camera, Scene* scene) override ;

    void Shutdown() override ;

private:
    Proxy<LayerManager> layerManager;
    Proxy<TimeManager> timeManager;

    unsigned int vao = 0;
    unsigned int vbo = 0;
    int viewportWidth = 0;
    int viewportHeight = 0;
};
