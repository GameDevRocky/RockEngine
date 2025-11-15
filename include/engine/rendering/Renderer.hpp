// Renderer.h
#pragma once
#include <glad/glad.h>
#include <vector>
#include <string>
#include "engine/core/System.hpp"

class Renderer : public System {
public:
    Renderer();
    ~Renderer();

    // Singleton access
    static Renderer& Get() {
        static Renderer instance;
        return instance;
    }

    // Initialize OpenGL resources
    void Init();  // changed to bool for error checking

    // Clear framebuffer
    void Clear(float r, float g, float b, float a);

    // Render scene / draw calls
    void Render();

    // Framebuffer helpers
    void CreateFramebuffer(int width, int height);
    void ResizeFramebuffer(int width, int height); // <--- added
    void Shutdown(); // cleanup resources

    GLuint GetFramebufferTexture() const { return fboTexture; }

private:
    GLuint fbo = 0;
    GLuint fboTexture = 0;
    GLuint rbo = 0;

    // Shaders
    GLuint shaderProgram = 0;
    bool LoadShaders(const std::string& vertexSrc, const std::string& fragSrc);

    // VAO/VBO for a simple quad (for postprocessing / testing)
    GLuint quadVAO = 0;
    GLuint quadVBO = 0;

    void SetupQuad();
};
