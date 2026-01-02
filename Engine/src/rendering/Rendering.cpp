// Renderer.cpp
#include "engine/rendering/Renderer.hpp"
#include "engine/core/TimeManager.hpp"
#include "engine/debug/Console.hpp"
#include <iostream>
#include <cmath> 
#include <chrono>

Renderer::Renderer() {}
Renderer::~Renderer() {
    Shutdown();
}

// --------------------
// Initialize OpenGL resources
// --------------------
void Renderer::Init() {
    // Load GLAD (context must be current before calling)
    if (!gladLoadGL()) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
    }

    glEnable(GL_DEPTH_TEST);

    // Setup a quad for post-processing / FBO display
    SetupQuad();

    // Simple shader
    // Simple color-changing triangle shader
const std::string vertexShaderSrc = R"(#version 460 core
layout(location = 0) in vec3 aPos;
void main() {
    gl_Position = vec4(aPos, 1.0);
})";

const std::string fragShaderSrc = R"(#version 460 core
out vec4 FragColor;
uniform float time;
void main() {
    float r = abs(sin(time));
    float g = abs(sin(time + 2.0));
    float b = abs(sin(time + 4.0));
    FragColor = vec4(r, g, b, 1.0);
})";

if (!LoadShaders(vertexShaderSrc, fragShaderSrc)) {
    std::cerr << "Failed to load shaders" << std::endl;
}

// Setup triangle
float triangleVertices[] = {
     0.0f,  0.5f, 0.0f,
    -0.5f, -0.5f, 0.0f,
     0.5f, -0.5f, 0.0f
};

glGenVertexArrays(1, &quadVAO);  // reuse quadVAO for simplicity
glGenBuffers(1, &quadVBO);
glBindVertexArray(quadVAO);
glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
glBufferData(GL_ARRAY_BUFFER, sizeof(triangleVertices), triangleVertices, GL_STATIC_DRAW);

glEnableVertexAttribArray(0);
glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

glBindBuffer(GL_ARRAY_BUFFER, 0);
glBindVertexArray(0);


}

void Renderer::Clear(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::Render() {
    float time = TimeManager::Get().ElapsedTime();

    // Assume the caller (QOpenGLWidget::paintGL) has already bound the correct framebuffer.
    // Query current viewport in case we need it:
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    // vp[2] = width, vp[3] = height

    // Optionally ensure viewport matches expected size:
    // glViewport(0, 0, vp[2], vp[3]);

    // Clear current framebuffer (Qt's FBO)
    Clear(sin(time), 0.7f, 0.7f, 1.0f);

    // Use our shader
    glUseProgram(shaderProgram);

    // Pass time to shader
    GLint loc = glGetUniformLocation(shaderProgram, "time");
    if (loc >= 0)
        glUniform1f(loc, time);

    // Draw triangle
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    glUseProgram(0);
}




// --------------------
// Create framebuffer
// --------------------
void Renderer::CreateFramebuffer(int width, int height) {
    if (fbo) glDeleteFramebuffers(1, &fbo);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Color attachment
    if (fboTexture) glDeleteTextures(1, &fboTexture);
    glGenTextures(1, &fboTexture);
    glBindTexture(GL_TEXTURE_2D, fboTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTexture, 0);

    // Depth + stencil
    if (rbo) glDeleteRenderbuffers(1, &rbo);
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "Framebuffer not complete!" << std::endl;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// --------------------
// Resize framebuffer
// --------------------
void Renderer::ResizeFramebuffer(int width, int height) {
    if (!fbo) {
        CreateFramebuffer(width, height);
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Resize color texture
    glBindTexture(GL_TEXTURE_2D, fboTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

    // Resize depth/stencil
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// --------------------
// Cleanup all GL resources
// --------------------
void Renderer::Shutdown() {
    if (fbo) { glDeleteFramebuffers(1, &fbo); fbo = 0; }
    if (fboTexture) { glDeleteTextures(1, &fboTexture); fboTexture = 0; }
    if (rbo) { glDeleteRenderbuffers(1, &rbo); rbo = 0; }
    if (quadVBO) { glDeleteBuffers(1, &quadVBO); quadVBO = 0; }
    if (quadVAO) { glDeleteVertexArrays(1, &quadVAO); quadVAO = 0; }
    if (shaderProgram) { glDeleteProgram(shaderProgram); shaderProgram = 0; }
}

// --------------------
// Setup a quad
// --------------------
void Renderer::SetupQuad() {
    float quadVertices[] = {
        // positions      // texCoords
        -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 0.0f, 1.0f, 0.0f,

        -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 0.0f, 1.0f, 1.0f
    };

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

// --------------------
// Shader compilation
// --------------------
bool Renderer::LoadShaders(const std::string& vertexSrc, const std::string& fragSrc) {
    auto CompileShader = [](GLenum type, const std::string& src) -> GLuint {
        GLuint shader = glCreateShader(type);
        const char* cstr = src.c_str();
        glShaderSource(shader, 1, &cstr, nullptr);
        glCompileShader(shader);

        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            std::cerr << "Shader compilation error: " << infoLog << std::endl;
        }
        return shader;
    };

    GLuint vertex = CompileShader(GL_VERTEX_SHADER, vertexSrc);
    GLuint fragment = CompileShader(GL_FRAGMENT_SHADER, fragSrc);

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertex);
    glAttachShader(shaderProgram, fragment);
    glLinkProgram(shaderProgram);

    GLint success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        std::cerr << "Shader linking error: " << infoLog << std::endl;
        return false;
    }

    glDeleteShader(vertex);
    glDeleteShader(fragment);
    return true;
}
