#pragma once
#include <glad/glad.h>
#include "engine/rendering/passes/RenderPass.hpp"

class ClearPass : public RenderPass
{
public:
    ClearPass() = default;

    void Init() override {}
    void Resize(int width, int height) override {}

    void Execute(RenderCamera&, Scene&) override
    {
        glad_glClearColor(0.078f, 0.078f, 0.078f, 1.0f);
        glad_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }


    void Shutdown() override {}
};
