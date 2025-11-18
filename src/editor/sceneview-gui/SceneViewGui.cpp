#include "SceneViewGui.hpp"
#include "engine/rendering/RenderManager.hpp" 
#include <QDebug>
#include "engine/debug/Console.hpp"
#include "engine/rendering/cameras/SceneCamera.hpp"

SceneViewGui::SceneViewGui(QWidget* parent)
    : QOpenGLWidget(parent)
{
   

}


SceneViewGui::~SceneViewGui()
{
    makeCurrent();
    RenderManager::Get().Shutdown();
    doneCurrent();
}
void SceneViewGui::initializeGL() {
    initializeOpenGLFunctions();
    RenderManager::Get().Init();
    

    // Ensure pipeline FBO matches the real GL framebuffer size (consider HiDPI)
    int fbw = static_cast<int>(width() * devicePixelRatio());
    int fbh = static_cast<int>(height() * devicePixelRatio());
    RenderManager::Get().editor_pipeline->Resize(fbw, fbh);

    // -----------------------------
    // Fullscreen quad setup
    // -----------------------------
    float quadVertices[] = {
        // positions   // texcoords
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

    glBindVertexArray(0);

    // Shader for displaying the FBO texture
    const char* quadVert = R"(#version 460 core
    layout(location = 0) in vec3 aPos;
    layout(location = 1) in vec2 aTexCoord;
    out vec2 TexCoord;
    void main() {
        TexCoord = aTexCoord;
        gl_Position = vec4(aPos, 1.0);
    })";

    const char* quadFrag = R"(#version 460 core
    in vec2 TexCoord;
    out vec4 FragColor;
    uniform sampler2D uTexture;
    void main() {
        FragColor = texture(uTexture, TexCoord);
    })";

    // Compile & link
    quadShader = glCreateProgram();
    GLuint v = glCreateShader(GL_VERTEX_SHADER);
    GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(v, 1, &quadVert, nullptr);
    glCompileShader(v);
    glShaderSource(f, 1, &quadFrag, nullptr);
    glCompileShader(f);
    glAttachShader(quadShader, v);
    glAttachShader(quadShader, f);
    glLinkProgram(quadShader);
    glDeleteShader(v);
    glDeleteShader(f);
}


void SceneViewGui::resizeGL(int w, int h) {
    // Use actual framebuffer pixel size (widget size * devicePixelRatio)
    int fbw = static_cast<int>(w * devicePixelRatio());
    int fbh = static_cast<int>(h * devicePixelRatio());

    RenderManager::Get().editor_pipeline->Resize(fbw, fbh);
    SceneCamera::Get().Resize(fbw, fbh);
    glViewport(0, 0, fbw, fbh);
}

void SceneViewGui::paintGL() {
    makeCurrent();
    RenderManager::Get().Render();
    GLuint tex = RenderManager::Get().editor_pipeline->GetOutputTexture();

    // Ensure we're drawing to the Qt-provided default framebuffer (it may not be 0)
    GLuint qtFBO = static_cast<GLuint>(defaultFramebufferObject());
    glBindFramebuffer(GL_FRAMEBUFFER, qtFBO);

    // Default framebuffer may also be scaled on HiDPI displays
    int fbw = static_cast<int>(width() * devicePixelRatio());
    int fbh = static_cast<int>(height() * devicePixelRatio());
    glViewport(0, 0, fbw, fbh);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(quadShader);
    glBindVertexArray(quadVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(glGetUniformLocation(quadShader, "uTexture"), 0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glUseProgram(0);
}
  

void SceneViewGui::Init(){
    
}