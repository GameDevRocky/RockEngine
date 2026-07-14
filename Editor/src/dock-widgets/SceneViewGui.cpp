#include "dock-widgets/SceneViewGui.hpp"
#include <QDebug>
#include <QOpenGLContext>
#include <QSurfaceFormat>
#include "engine/debug/Console.hpp"
#include "engine/core/InputManager.hpp"
#include "engine/core/SelectionManager.hpp"
#include <glm/glm.hpp>
#include "engine/rendering/passes/ClearPass.hpp"
#include "engine/rendering/passes/ScenePass.hpp"
#include "engine/rendering/passes/GridPass.hpp"
#include "engine/rendering/passes/DebugPass.hpp"
#include "engine/rendering/passes/PickingPass.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/rendering/core/AssetMetaService.hpp"
#include "engine/rendering/core/GizmosManager.hpp"
#include "engine/components/BoxCollider.hpp"
#include "engine/components/CircleCollider.hpp"
#include "engine/components/CapsuleCollider.hpp"
#include "Engine.hpp"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "engine/core/TimeManager.hpp"
#include "utils/IconMaps.h" 

namespace {
    void DrawFPS(){
        
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 window_pos = ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - 10.0f, 
            viewport->WorkPos.y + 10.0f);
            ImGuiIO& io = ImGui::GetIO();
            TimeManager* timeManager = Engine::Get()->GetActiveContainer()->FindSystem<TimeManager>();
            float dt = timeManager->UnscaledDeltaTime();
            io.DeltaTime = (dt > 0.0f) ? dt : (1.0f / 60.0f);
            ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, ImVec2(1.0f, 0.0f));
            ImGui::SetNextWindowBgAlpha(0.35f); 
            ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | 
            ImGuiWindowFlags_AlwaysAutoResize | 
            ImGuiWindowFlags_NoSavedSettings | 
            ImGuiWindowFlags_NoFocusOnAppearing | 
            ImGuiWindowFlags_NoNav | 
            ImGuiWindowFlags_NoMove;

            if (ImGui::Begin("FPS Overlay", nullptr, window_flags)) {
                ImGui::Text("FPS: %.1f", timeManager->GetFPS());
                ImGui::End();
            }
        }
    }



    
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
    pickingPass = new PickingPass();
    
    renderPipeline->AddSetupPass(clearPass);
    renderPipeline->AddSetupPass(gridPass);

    renderPipeline->AddScenePass(scenePass);
    renderPipeline->AddScenePass(debugPass);
    renderPipeline->AddFinalizePass(pickingPass);

    renderPipeline->Init();
    camera->Init();

    renderPipeline->Resize(fbw, fbh);
    camera->Resize(fbw, fbh);
}
 
void SceneViewGui::initializeGL() {
    initializeOpenGLFunctions();
    this->installEventFilter(this);
    if (!gladLoadGL()) {
        QSurfaceFormat fmt = context()->format();
        std::cerr << "Failed to initialize GLAD (SceneView). Context obtained: "
                   << fmt.majorVersion() << "." << fmt.minorVersion()
                   << (fmt.profile() == QSurfaceFormat::CoreProfile ? " core" : " non-core")
                   << " | GL_VENDOR=" << reinterpret_cast<const char*>(glGetString(GL_VENDOR))
                   << " | GL_RENDERER=" << reinterpret_cast<const char*>(glGetString(GL_RENDERER))
                   << " | GL_VERSION=" << reinterpret_cast<const char*>(glGetString(GL_VERSION))
                   << std::endl;
        return;
    }
    imGuiInstance = new ImGuiInstance();
    imGuiInstance->Init();
    imGuiInstance->AddDrawCall([this](){DrawGizmos();});
    imGuiInstance->AddDrawCall([this](){DrawFPS();});
    imGuiInstance->AddDrawCall([this](){DrawToolBar();});
    
    AssetManager::Get().Init();
    AssetManager::Get().Awake();

    // Scan Domain/ for missing meta files, then load all assets from meta files.
    const std::string domainDir = EngineUtils::GetAssetPath("Domain");
    AssetMetaService::ScanAndGenerate(domainDir);
    AssetManager::Get().LoadFromDirectory(domainDir);

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
    if (imGuiInstance) imGuiInstance->Resize(w, h, devicePixelRatioF());
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
    
    if (imGuiInstance) imGuiInstance->Render();
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

    if (event->key() == Qt::Key_Delete) {
        auto* selMgr = container->FindSystem<SelectionManager>();
        if (selMgr) {
            GameObject* go = dynamic_cast<GameObject*>(selMgr->GetSerializable());
            if (go) {
                go->Shutdown();
                return;
            }
        }
    }

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
    glm::vec2 delta = before - after;
    camera->SetPosition(camera->GetPosition() + delta);
    event->accept();
}

bool SceneViewGui::eventFilter(QObject *obj, QEvent *event) {
    ImGuiIO& io = ImGui::GetIO();
    if (event->type() == QEvent::MouseMove) {
        auto* me = static_cast<QMouseEvent*>(event);
        io.MousePos = ImVec2(me->pos().x(), me->pos().y());
    }

    switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto* me = static_cast<QMouseEvent*>(event);
            io.MousePos = ImVec2(me->pos().x(), me->pos().y());
            if (me->button() == Qt::LeftButton) io.MouseDown[0] = true;
            if (me->button() == Qt::RightButton) io.MouseDown[1] = true;
            
            if (io.WantCaptureMouse || GizmosManager::Get()->WantsCaptureMouse())
                return true;
            return false;
        }
        case QEvent::MouseButtonRelease: {
            auto* me = static_cast<QMouseEvent*>(event);
            io.MousePos = ImVec2(me->pos().x(), me->pos().y());
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
        // Don't deselect if GizmosManager is using a handle
        if (GizmosManager::Get()->WantsCaptureMouse())
            return;

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
    float ortho = camera->GetOrthoSize();

    float dpi = devicePixelRatioF();
    float viewportH = height() * dpi;

    float worldHeight = (ortho / zoom) * 2.0f;
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
    glm::mat4 invProj = glm::inverse(camera->GetProjectionMatrix());
    glm::mat4 invView = glm::inverse(camera->GetViewMatrix());

    glm::vec4 world = invView * (invProj * clip);

    return glm::vec2(world.x, world.y);
}

void SceneViewGui::Init(){

    std::cout << "SceneViewGui Initialized" << std::endl;
}

void SceneViewGui::DrawGizmos(){
    GizmosManager::Get()->DrawGizmos(
        camera->GetViewMatrix(),
        camera->GetProjectionMatrix(),
        static_cast<float>(width()),
        static_cast<float>(height()));
}

void SceneViewGui::DrawToolBar() {

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 2.0f)); 
    
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.12f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));

    if (ImGui::Begin("###Toolbar", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse))
    {
        ImGui::PopStyleVar();

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 winPos = ImGui::GetWindowPos();
        float winWidth = ImGui::GetWindowSize().x;
        float centerX = winPos.x + (winWidth * 0.5f);
        float lineY = winPos.y + 10.0f;
        ImU32 gripColor = ImGui::GetColorU32(ImGuiCol_TextDisabled, 0.5f);
        drawList->AddLine(ImVec2(centerX - 10, lineY), ImVec2(centerX + 10, lineY), gripColor, 1.5f);
        drawList->AddLine(ImVec2(centerX - 10, lineY + 4), ImVec2(centerX + 10, lineY + 4), gripColor, 1.5f);

        auto* gizmos = GizmosManager::Get();

        auto ToolButton = [&](const char* icon, ImGuizmo::OPERATION op) {
            bool is_active = (gizmos->GetOperation() == op) &&
                             (gizmos->GetEditMode() == GizmosManager::EditMode::Transform);
            
            if (is_active) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.58f, 0.26, 1.0f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            }

            if (ImGui::Button(icon, ImVec2(24, 24))) {
                gizmos->SetOperation(op);
                gizmos->SetEditMode(GizmosManager::EditMode::Transform);
            }
            ImGui::PopStyleColor();
        };

        ToolButton(ICON_FA_HAND, ImGuizmo::OPERATION(-1));
        ImGui::Separator();
        ToolButton(ICON_FA_ARROWS_TO_DOT, ImGuizmo::UNIVERSAL);
        ToolButton(ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT, ImGuizmo::TRANSLATE);
        ToolButton(ICON_FA_ROTATE, ImGuizmo::ROTATE);
        ToolButton(ICON_FA_MAXIMIZE, ImGuizmo::SCALE);

    
        auto* selectionManager = Engine::Get()->GetActiveContainer()->FindSystem<SelectionManager>();
        GameObject* obj = selectionManager ? dynamic_cast<GameObject*>(selectionManager->GetSerializable()) : nullptr;
        bool hasCollider = obj && (
            obj->GetComponent<BoxCollider>() ||
            obj->GetComponent<CircleCollider>() ||
            obj->GetComponent<CapsuleCollider>());

        if (hasCollider) {
            ImGui::Separator();
            bool is_collider = (gizmos->GetEditMode() == GizmosManager::EditMode::Collider);
            if (is_collider) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.40f, 0.58f, 1.0f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            }
            if (ImGui::Button(ICON_FA_DRAW_POLYGON, ImVec2(24, 24))) {
                if (is_collider) {
                    gizmos->SetEditMode(GizmosManager::EditMode::Transform);
                } else {
                    gizmos->SetEditMode(GizmosManager::EditMode::Collider);
                }
            }
            ImGui::PopStyleColor();
        } else if (gizmos->GetEditMode() == GizmosManager::EditMode::Collider) {
            gizmos->SetEditMode(GizmosManager::EditMode::Transform);
        }
    

        ImGui::End();
    }
    else {
        ImGui::PopStyleVar(); 
    }
    
    ImGui::PopStyleColor(3);
}
