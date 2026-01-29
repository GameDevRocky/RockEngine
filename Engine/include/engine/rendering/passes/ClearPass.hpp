#pragma once
#include <glad/glad.h>
#include "engine/rendering/passes/RenderPass.hpp"

class ClearPass : public RenderPass
{
public:
    ClearPass() = default;

    void Init() override {}
    void Resize(int width, int height) override {}

    void Execute(RenderCamera* camera, Scene* scene) override ;

    void Shutdown() override {}
};
