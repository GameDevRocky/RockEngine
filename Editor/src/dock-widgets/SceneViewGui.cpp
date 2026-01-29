#include "dock-widgets/SceneViewGui.hpp"
#include <QDebug>
#include "engine/debug/Console.hpp"
#include "engine/core/InputManager.hpp"
#include <glm/glm.hpp>
#include "engine/rendering/passes/ClearPass.hpp"
#include "engine/rendering/passes/ScenePass.hpp"
#include "engine/rendering/passes/GridPass.hpp"
#include "engine/rendering/core/SharedResources.hpp"
#include "Engine.hpp"

//#include "Engine.hpp";

SceneViewGui::SceneViewGui(QWidget* parent)
    : QOpenGLWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
setMouseTracking(true);

}


SceneViewGui::~SceneViewGui()
{
    makeCurrent();
    doneCurrent();
}

void SceneViewGui::initializeRenderPipeline(){
    int fbw = static_cast<int>(width() * devicePixelRatio());
    int fbh = static_cast<int>(height() * devicePixelRatio());
    
    renderPipeline = new RenderPipeline();
    camera = new RenderCamera();

    
    ClearPass* clearPass = new ClearPass();
    GridPass* gridPass = new GridPass();
    ScenePass* scenePass = new ScenePass();
    
    renderPipeline->AddSetupPass(clearPass);
    renderPipeline->AddSetupPass(gridPass);

    renderPipeline->AddScenePass(scenePass);
    
    renderPipeline->Init();
    camera->Init();

    renderPipeline->Resize(fbw, fbh);
    camera->Resize(fbw, fbh);
}
 
void SceneViewGui::initializeGL() {
    initializeOpenGLFunctions();

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
    std::cout << "SceneViewGui OpenGl Initialized. " << std::endl;
}


void SceneViewGui::resizeGL(int w, int h) {
    int fbw = static_cast<int>(w * devicePixelRatio());
    int fbh = static_cast<int>(h * devicePixelRatio());

    renderPipeline->Resize(fbw, fbh);
    camera->Resize(fbw, fbh);
    glViewport(0, 0, fbw, fbh);
}

void SceneViewGui::paintGL() {

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

void SceneViewGui::Render(){
    Engine* engine = Engine::Get();
    SceneManager* sceneManager = engine->GetActiveContainer()->FindSystem<SceneManager>();
    
    renderPipeline->Render(camera, sceneManager->GetScenes());
    
}


void SceneViewGui::keyPressEvent(QKeyEvent* event) {
    Engine* engine = Engine::Get();
    auto* container = engine->GetActiveContainer();
    InputManager* inputManager = engine->GetActiveContainer()->FindSystem<InputManager>();

    inputManager->SetKeyState(event->key(), true);
    Console::Comment(std::to_string(event->key()));
}

void SceneViewGui::keyReleaseEvent(QKeyEvent* event) {
    Engine* engine = Engine::Get();
    InputManager* inputManager = engine->GetActiveContainer()->FindSystem<InputManager>();
    inputManager->SetKeyState(event->key(), false);
    Console::Comment(std::to_string(event->key()));
}


void SceneViewGui::wheelEvent(QWheelEvent* event)
{
    QPoint mousePos = event->position().toPoint();

    glm::vec2 before = ScreenToWorld(mousePos);

    float scroll = event->angleDelta().y();
    float factor = 1.0f + scroll / 1200.0f;

    float newZoom = camera->GetZoom() * factor;
    camera->SetZoom(newZoom);

    glm::vec2 after = ScreenToWorld(mousePos);

    // Camera must shift so world point under cursor stays fixed
    glm::vec2 delta = before - after;

    camera->SetPosition(camera->GetPosition() + delta);

    event->accept();
}



void SceneViewGui::mousePressEvent(QMouseEvent* event)
{
    bool ctrlLeftPan = (event->button() == Qt::LeftButton &&
                        (event->modifiers() & Qt::ControlModifier));

    if (event->button() == Qt::MiddleButton || ctrlLeftPan)
    {
        isPanning = true;
        lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
}


void SceneViewGui::mouseReleaseEvent(QMouseEvent* event)
{
    bool ctrlLeftPan = (event->button() == Qt::LeftButton &&
                        (event->modifiers() & Qt::ControlModifier));

    if (event->button() == Qt::MiddleButton || ctrlLeftPan)
    {
        isPanning = false;
        setCursor(Qt::ArrowCursor);
    }
}


void SceneViewGui::mouseMoveEvent(QMouseEvent* e)
{
    Engine* engine = Engine::Get();
    InputManager* inputManager = engine->GetActiveContainer()->FindSystem<InputManager>();

    bool ctrlHeld = (e->modifiers() & Qt::ControlModifier);
    bool leftDragPan = (e->buttons() & Qt::LeftButton) && ctrlHeld;
    bool midDragPan  = (e->buttons() & Qt::MiddleButton);

    glm::vec2 currentPos = { static_cast<float>(e->pos().x()), static_cast<float>(e->pos().y()) };
    inputManager->SetMousePosition(currentPos);

    if (!leftDragPan && !midDragPan) {
        lastMousePos = e->pos();
        return;
    }

    QPoint deltaPx = e->pos() - lastMousePos;

    float zoom = camera->GetZoom();
    float ortho = camera->GetOrthoSize(); // whatever you named it

    float dpi = devicePixelRatioF();
    float viewportH = height() * dpi;

    // how many world units fit on screen vertically
    float worldHeight = (ortho / zoom) * 2.0f;

    // world units per pixel
    float unitsPerPixel = worldHeight / viewportH;

    glm::vec2 worldDelta(
        -deltaPx.x() * unitsPerPixel,
         deltaPx.y() * unitsPerPixel
    );

    camera->SetPosition(
        camera->GetPosition() + worldDelta
    );

    lastMousePos = e->pos();
}


glm::vec2 SceneViewGui::ScreenToWorld(const QPoint& p)
{
    float px = p.x() * devicePixelRatioF();
    float py = p.y() * devicePixelRatioF();

    int w = width() * devicePixelRatioF();
    int h = height() * devicePixelRatioF();

    float ndcX = (px / w) * 2.0f - 1.0f;
    float ndcY = 1.0f - (py / h) * 2.0f; 

    glm::vec4 clip(ndcX, ndcY, 0.0f, 1.0f);

    // Convert through inverse projection and inverse view
    glm::mat4 invProj = glm::inverse(camera->GetProjectionMatrix());
    glm::mat4 invView = glm::inverse(camera->GetViewMatrix());

    glm::vec4 world = invView * (invProj * clip);

    return glm::vec2(world.x, world.y);
}



void SceneViewGui::Init(){
    std::cout << "SceneViewGui Initialized" << std::endl;
}
