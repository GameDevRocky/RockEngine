#pragma once
#include "engine/rendering/cameras/RenderCamera.hpp"
#include "engine/core/Scene.hpp"
#include "engine/core/Observable.hpp"

class RenderPass : public Observable
{
public:
    virtual void Init(){};
    virtual void Resize(int width, int height){};
    virtual void Execute(RenderCamera* camera, Scene* scene){};
    virtual void Shutdown() = 0;

    // Optional: passes that output a texture override this
    virtual unsigned int GetOutputTexture() const { return 0; }

    virtual ~RenderPass() = default;

protected: 
    bool enabled = true;

};
