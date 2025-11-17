#pragma once
#include "engine/rendering/passes/RenderPass.hpp"
#include <glad/glad.h>

class ClearPass : public RenderPass
{
public:
    ClearPass() = default;

    void Init() override {}
    void Resize(int width, int height) override {}

    void Execute(RenderCamera&, Scene&) override
    {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // ✅ FIX
    }


    void Shutdown() override {}
};
