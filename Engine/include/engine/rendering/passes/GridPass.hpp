#pragma once
#include <glad/glad.h>

#include "engine/rendering/passes/RenderPass.hpp"
#include "engine/rendering/cameras/RenderCamera.hpp"
#include "engine/core/Scene.hpp"
#include "engine/rendering/core/Shader.hpp"
#include "engine/core/TimeManager.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/debug/Console.hpp"
#include <memory>
#include "Engine.hpp"

class GridPass : public RenderPass
{
public:
    GridPass() = default;
    ~GridPass() override { Shutdown(); }

    void Init() override ;
    void Execute(RenderCamera* camera, Scene* scene) override;
    void Shutdown() override;

private:
    GLuint vao = 0;
    GLuint vbo = 0;
    Shader* shader = nullptr;
};
