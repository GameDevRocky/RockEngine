#pragma once
#include "engine/rendering/cameras/RenderCamera.hpp"
#include "engine/core/Scene.hpp"

class RenderPass
{
public:
    virtual void Init() = 0;
    virtual void Resize(int width, int height) = 0;
    virtual void Execute(RenderCamera& camera, Scene& scene) = 0;
    virtual void Shutdown() = 0;

    // Optional: passes that output a texture override this
    virtual unsigned int GetOutputTexture() const { return 0; }

    virtual ~RenderPass() = default;
};
