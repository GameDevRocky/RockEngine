#pragma once
#include "engine/rendering/cameras/RenderCamera.hpp"
#include "engine/core/Scene.hpp"
#include "engine/core/Observable.hpp"

class RenderPass : public Observable
{
public:
    // Resource-ownership rule: shaders/textures/materials obtained from
    // AssetManager are BORROWED. A pass must never delete them in Shutdown() --
    // ~Shader calls glDeleteProgram, so deleting a borrowed shader destroys the
    // shared GL program and dangles it for every other consumer. Null the pointer
    // instead. A pass owns (and must delete) only GL objects it created itself
    // (its own VAOs/VBOs/SSBOs).
    virtual void Init(){};
    virtual void Resize(int width, int height){};
    virtual void Execute(RenderCamera* camera, Scene* scene){};
    virtual void Shutdown() = 0;

    virtual ~RenderPass() = default;

protected: 
    bool enabled = true;

};
