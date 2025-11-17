#pragma once
#include "engine/rendering/passes/RenderPass.hpp"
#include <glad/glad.h>

class ClearPass : public RenderPass
{
public:
    ClearPass() = default;

    void Init() override {}
    void Resize(int width, int height) override {}

    void Execute(RenderCamera& camera, Scene& scene) override
    {
        glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    void Shutdown() override {}
};
