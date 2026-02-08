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
    unsigned int circleVao = 0;
    unsigned int circleVbo = 0;
    unsigned int capsuleVao = 0;
    unsigned int capsuleVbo = 0;
    unsigned int topSemiVao = 0;
    unsigned int topSemiVbo = 0;
    unsigned int bottomSemiVao = 0;
    unsigned int bottomSemiVbo = 0;
    Shader* debugShader = nullptr;
    int viewportWidth = 0;
    int viewportHeight = 0;
};
