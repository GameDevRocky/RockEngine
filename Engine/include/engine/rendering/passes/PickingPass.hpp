#pragma once
#include "engine/rendering/passes/RenderPass.hpp"
#include "engine/rendering/core/Shader.hpp"
#include <glad/glad.h>

class PickingPass : public RenderPass {
public:
    void Init() override;
    void Execute(RenderCamera* camera, Scene* scene) override;
    void Resize(int width, int height) override;
    void Shutdown() override;

    uint32_t ReadPixel(int x, int y);

private:
    Shader* shader = nullptr;
    GLuint fbo = 0;
    GLuint pickingTexture = 0;
    GLuint depthTexture = 0;
    
    // Quad for sprites (shared with ScenePass notionally, but we create our own to be safe)
    GLuint vao = 0; 
    GLuint vbo = 0;
    
    int viewportWidth = 0;
    int viewportHeight = 0;
};
