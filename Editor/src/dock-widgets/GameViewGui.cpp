#include "dock-widgets/GameViewGui.hpp"
#include <QDebug>
#include "engine/debug/Console.hpp"
#include "engine/core/InputManager.hpp"
#include <glm/glm.hpp>
#include "engine/rendering/passes/ClearPass.hpp"
#include "engine/rendering/passes/ScenePass.hpp"
#include "engine/rendering/passes/GridPass.hpp"
#include "engine/rendering/core/SharedResources.hpp"
#include "Engine.hpp"

GameViewGui::GameViewGui(QWidget* parent)
    : QOpenGLWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
setMouseTracking(true);

}
void GameViewGui::Init(){ 
    resize(300, 500);
    std::cout << "GameViewGui Initialized" << std::endl;
}  

void GameViewGui::initializeRenderPipeline(){
    int fbw = static_cast<int>(width() * devicePixelRatio());
    int fbh = static_cast<int>(height() * devicePixelRatio());
    
    renderPipeline = new RenderPipeline();
    camera = new RenderCamera();

    
    ClearPass* clearPass = new ClearPass();
    ScenePass* scenePass = new ScenePass();
    
    renderPipeline->AddPass(clearPass);
    renderPipeline->AddPass(scenePass);
    
    renderPipeline->Init();
    camera->Init();

    renderPipeline->Resize(fbw, fbh);
    camera->Resize(fbw, fbh);
}

void GameViewGui::initializeGL() {
    initializeOpenGLFunctions();

    // We use glad_gl* calls throughout the renderer/passes; GLAD must be loaded
    // after a context is current (Qt makes the context current for initializeGL).
    if (!gladLoadGL()) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return;
    }

    SharedResources::Get().Init();
    initializeRenderPipeline();

    float quadVertices[] = {
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

void GameViewGui::resizeGL(int w, int h) {
    int fbw = static_cast<int>(w * devicePixelRatio());
    int fbh = static_cast<int>(h * devicePixelRatio());

    renderPipeline->Resize(fbw, fbh);
    camera->Resize(fbw, fbh);
    glViewport(0, 0, fbw, fbh);
}

void GameViewGui::paintGL() {

    makeCurrent();
    Render();
    GLuint tex = renderPipeline->GetOutputTexture();
    GLuint qtFBO = static_cast<GLuint>(defaultFramebufferObject());
    glBindFramebuffer(GL_FRAMEBUFFER, qtFBO);

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

void GameViewGui::Render(){
    Engine* engine = Engine::Get();
    auto* container = engine->GetActiveContainer();
    SceneManager* sceneManager = engine->GetActiveContainer()->FindSystem<SceneManager>();

    for (auto& scene : sceneManager->GetScenes()){
        renderPipeline->Render(*camera, *scene);
    }    
}