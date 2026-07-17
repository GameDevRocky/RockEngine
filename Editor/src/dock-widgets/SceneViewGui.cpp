#include "dock-widgets/SceneViewGui.hpp"
#include "engine/core/InputManager.hpp"
#include "engine/core/SelectionManager.hpp"
#include <glm/glm.hpp>
#include "engine/rendering/Renderer.hpp"
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
    : ViewportWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

RenderView* SceneViewGui::CreateView(int pixelW, int pixelH)
{
    editorView = Renderer::Get().CreateEditorView(pixelW, pixelH);
    return editorView;
}

void SceneViewGui::OnViewInitialized()
{
    // Must precede imGuiInstance construction with no intervening event-loop
    // turn: eventFilter() unconditionally calls ImGui::GetIO(), which asserts
    // if a filtered event arrives before ImGui::CreateContext() has run.
    installEventFilter(this);

    imGuiInstance = new ImGuiInstance();
    imGuiInstance->Init();
    imGuiInstance->AddDrawCall([this](){DrawGizmos();});
    imGuiInstance->AddDrawCall([this](){DrawFPS();});
    imGuiInstance->AddDrawCall([this](){DrawToolBar();});

    std::cout << "SceneViewGui OpenGl Initialized. " << std::endl;
}

void SceneViewGui::OnResized(int w, int h)
{
    if (imGuiInstance) imGuiInstance->Resize(w, h, devicePixelRatioF());
}

void SceneViewGui::OnAfterPresent()
{
    if (imGuiInstance) imGuiInstance->Render();
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
    glm::vec2 fbPos(mousePos.x() * devicePixelRatioF(), mousePos.y() * devicePixelRatioF());
    float scroll = event->angleDelta().y();
    editorView->GetEditorCamera()->ZoomAt(fbPos, scroll);
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

        std::string objectId = editorView->Pick(fbX, fbY);

        auto* selMgr = Engine::Get()->GetActiveContainer()->FindSystem<SelectionManager>();

        if (!objectId.empty()) {
            selMgr->Select(objectId);
            std::cout << "Selected GameObject: " << objectId << std::endl;
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

    glm::vec2 fbPos(e->pos().x() * devicePixelRatioF(), e->pos().y() * devicePixelRatioF());
    glm::vec2 worldPos = editorView->ScreenToWorld(fbPos);
    inputManager->SetMousePosition(worldPos);

    if (!leftDragPan && !midDragPan) {
        lastMousePos = e->pos();
        return;
    }

    QPoint deltaPx = e->pos() - lastMousePos;
    editorView->GetEditorCamera()->PanByPixels(glm::vec2(deltaPx.x(), deltaPx.y()));

    lastMousePos = e->pos();
}


void SceneViewGui::Init(){

    std::cout << "SceneViewGui Initialized" << std::endl;
}

void SceneViewGui::DrawGizmos(){
    RenderCamera* camera = editorView->GetCamera();
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
