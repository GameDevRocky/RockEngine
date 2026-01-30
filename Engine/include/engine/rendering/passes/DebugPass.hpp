#pragma once
#include <glad/glad.h>
#include "engine/rendering/passes/RenderPass.hpp"
#include "engine/core/Scene.hpp"
#include "engine/rendering/core/Shader.hpp"
#include <vector>
#include <glm/glm.hpp>

class DebugPass : public RenderPass
{
public:
    DebugPass() = default;

    void Init() override;

    void Resize(int width, int height) override;

    void Execute(RenderCamera* camera, Scene* scene) override ;

    void Shutdown() override ;

private:
    unsigned int vao = 0;
    unsigned int vbo = 0;
    Shader* debugShader = nullptr;
    int viewportWidth = 0;
    int viewportHeight = 0;
};
