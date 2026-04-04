#include "dock-widgets/SceneViewGui.hpp"
#include <QDebug>
#include "engine/debug/Console.hpp"
#include "engine/core/InputManager.hpp"
#include "engine/core/SelectionManager.hpp"
#include <glm/glm.hpp>
#include "engine/rendering/passes/ClearPass.hpp"
#include "engine/rendering/passes/ScenePass.hpp"
#include "engine/rendering/passes/GridPass.hpp"
#include "engine/rendering/passes/DebugPass.hpp"
#include "engine/rendering/passes/PickingPass.hpp"
#include "engine/rendering/passes/ImGuiPass.hpp"
#include "engine/rendering/core/SharedResources.hpp"
#include "Engine.hpp"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#define RESOURCES_CONFIG_PATH PROJECT_ROOT "/Domain/lib/configs/resources_config.yaml"



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
    DebugPass* debugPass = new DebugPass();
    ImGuiPass* imGuiPass = new ImGuiPass();
    pickingPass = new PickingPass();
    
    renderPipeline->AddSetupPass(clearPass);
    renderPipeline->AddSetupPass(gridPass);

    renderPipeline->AddScenePass(scenePass);
    renderPipeline->AddScenePass(debugPass);
    renderPipeline->AddFinalizePass(pickingPass);
    pickingPass->SetDebugDraw(false);  // Enable debug visualization
    
    renderPipeline->AddScenePass(imGuiPass);
    
    renderPipeline->Init();
    camera->Init();

    renderPipeline->Resize(fbw, fbh);
    camera->Resize(fbw, fbh);
}
 
void SceneViewGui::initializeGL() {
    initializeOpenGLFunctions();
    this->installEventFilter(this); 
    if (!gladLoadGL()) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return;
    }
    
    SharedResources::Get().Deserialize(YAML::LoadFile(RESOURCES_CONFIG_PATH));
    SharedResources::Get().Init();
    SharedResources::Get().Awake();
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
}

void SceneViewGui::keyReleaseEvent(QKeyEvent* event) {
    Engine* engine = Engine::Get();
    InputManager* inputManager = engine->GetActiveContainer()->FindSystem<InputManager>();
    inputManager->SetKeyState(event->key(), false);
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

bool SceneViewGui::eventFilter(QObject *obj, QEvent *event) {
    ImGuiIO& io = ImGui::GetIO();
    float dpi = devicePixelRatioF();
    if (event->type() == QEvent::MouseMove) {
        auto* me = static_cast<QMouseEvent*>(event);
        io.MousePos = ImVec2(me->pos().x() * dpi, me->pos().y() * dpi);
    }

    switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto* me = static_cast<QMouseEvent*>(event);
            io.MousePos = ImVec2(me->pos().x() * dpi, me->pos().y() * dpi);
            if (me->button() == Qt::LeftButton) io.MouseDown[0] = true;
            if (me->button() == Qt::RightButton) io.MouseDown[1] = true;
            
            return io.WantCaptureMouse; 
        }
        case QEvent::MouseButtonRelease: {
            auto* me = static_cast<QMouseEvent*>(event);
            io.MousePos = ImVec2(me->pos().x() * dpi, me->pos().y() * dpi);
            if (me->button() == Qt::LeftButton) io.MouseDown[0] = false;
            if (me->button() == Qt::RightButton) io.MouseDown[1] = false;
            return io.WantCaptureMouse;
        }
        case QEvent::Wheel: {
            auto* we = static_cast<QWheelEvent*>(event);
            io.MouseWheel += we->angleDelta().y() / 120.0f;
            return io.WantCaptureMouse;
        }
        default:
            break;
    }
    return QOpenGLWidget::eventFilter(obj, event);
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
        return;
    }
    
    // Left click without modifier - try to select object
    if (event->button() == Qt::LeftButton)
    {
        makeCurrent();  // Ensure OpenGL context is active for ReadPixel
        
        float dpi = devicePixelRatioF();
        int fbX = static_cast<int>(event->pos().x() * dpi);
        int fbY = static_cast<int>((height() - event->pos().y()) * dpi);  // Flip Y for OpenGL
        
        uint32_t pickId = pickingPass->ReadPixel(fbX, fbY);
        
        auto* selMgr = Engine::Get()->GetActiveContainer()->FindSystem<SelectionManager>();
        
        if (pickId > 0) {
            std::string objectId = pickingPass->GetPickedObjectId(pickId);
            if (!objectId.empty()) {
                selMgr->Select(objectId);
                std::cout << "Selected GameObject: " << objectId << std::endl;
            }
        } else {
            selMgr->Deselect();
            std::cout << "Selection cleared" << std::endl;
        }
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

    // Get screen coordinates
    glm::vec2 screenPos = { static_cast<float>(e->pos().x()), static_cast<float>(e->pos().y()) };
    
    // Convert to world coordinates using widget dimensions (not framebuffer dimensions)
    glm::vec2 worldPos = camera->ScreenToWorld(screenPos, width(), height());
    
    // Set the world position in InputManager
    inputManager->SetMousePosition(worldPos);

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
