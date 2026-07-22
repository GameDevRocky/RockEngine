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
#include "engine/core/UndoSystem.hpp"
#include "engine/commands/SubtreeCommand.hpp"
#include "engine/commands/MacroCommand.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/core/Scene.hpp"
#include "engine/core/GameObject.hpp"
#include <yaml-cpp/yaml.h>
#include <memory>
#include <vector>

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
        auto* registry = container->FindSystem<Registry>();
        if (selMgr && registry && selMgr->HasSelection()) {
            // Roots only: destroying a parent already destroys its children, and a
            // child's id would no longer resolve by the time we reached it.
            std::vector<std::string> roots = selMgr->GetSelectedRoots();
            auto* undoSystem = container->FindSystem<UndoSystem>();

            std::vector<std::unique_ptr<Command>> commands;
            commands.reserve(roots.size());
            for (const std::string& id : roots) {
                GameObject* go = registry->Find<GameObject>(id);
                if (!go) continue;
                // DestroyAndRecord performs the Shutdown so it can snapshot first,
                // but returns nullptr WITHOUT destroying when the object has no
                // scene — the branches are not interchangeable.
                if (undoSystem) {
                    if (auto command = SubtreeCommand::DestroyAndRecord(go))
                        commands.push_back(std::move(command));
                } else {
                    go->Shutdown();
                }
            }

            if (undoSystem) {
                auto macro = MacroCommand::Wrap(
                    std::move(commands),
                    roots.size() == 1
                        ? "Delete Object"
                        : "Delete " + std::to_string(roots.size()) + " Objects");
                if (auto* m = dynamic_cast<MacroCommand*>(macro.get())) {
                    m->SetSelectionAfterUndo(roots);
                    m->SetSelectionAfterRedo({});
                }
                undoSystem->Push(std::move(macro));
            }

            selMgr->ClearSelection();
            return;
        }
    }

    // Ctrl+D: duplicate the selection -- same behaviour as the Hierarchy tree
    // (see SceneTree::DuplicateSelection), so it works while the Scene view is focused.
    if (event->key() == Qt::Key_D && (event->modifiers() & Qt::ControlModifier)) {
        auto* selMgr = container->FindSystem<SelectionManager>();
        auto* registry = container->FindSystem<Registry>();
        if (selMgr && registry && selMgr->HasSelection()) {
            // Roots only: duplicating a parent already clones its children, so a
            // selected child would otherwise produce a stray second copy.
            std::vector<std::string> roots = selMgr->GetSelectedRoots();
            auto* undoSystem = container->FindSystem<UndoSystem>();

            std::vector<std::unique_ptr<Command>> commands;
            std::vector<std::string> cloneIds;
            commands.reserve(roots.size());
            for (const std::string& id : roots) {
                GameObject* source = registry->Find<GameObject>(id);
                if (!source) continue;
                Scene* scene = source->GetScene();
                if (!scene) continue;
                // Capture the post-remap YAML so redo rebuilds this same clone
                // instead of minting fresh ids.
                YAML::Node snapshot;
                GameObject* clone = scene->DuplicateGameObject(source, &snapshot);
                if (!clone) continue;
                cloneIds.push_back(clone->GetID());
                if (undoSystem)
                    commands.push_back(SubtreeCommand::RecordCreated(
                        clone, snapshot, "Duplicate " + source->GetName()));
            }
            if (cloneIds.empty()) return;

            selMgr->SelectMany(cloneIds);

            if (undoSystem && !commands.empty()) {
                auto macro = MacroCommand::Wrap(
                    std::move(commands),
                    cloneIds.size() == 1
                        ? "Duplicate Object"
                        : "Duplicate " + std::to_string(cloneIds.size()) + " Objects");
                if (auto* m = dynamic_cast<MacroCommand*>(macro.get())) {
                    m->SetSelectionAfterUndo(roots);      // undo removes the clones
                    m->SetSelectionAfterRedo(cloneIds);   // redo brings them back
                }
                undoSystem->Push(std::move(macro));
            }
            return;
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
    if (!imGuiInstance) return QOpenGLWidget::eventFilter(obj, event);

    // Two ImGui contexts are live (Scene + Game); make ours current before
    // touching its IO. Without this, ImGui::GetIO() returns whichever context
    // rendered last -- so with both viewports visible at once, a mouse event
    // over the Scene view would write MousePos/MouseDown into the *Game* view's
    // IO (input bleeding between the two panels). See GameViewGui::eventFilter.
    imGuiInstance->MakeCurrent();
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

bool SceneViewGui::IsPanGesture(Qt::MouseButton button, Qt::KeyboardModifiers modifiers) const
{
    // Alt+Left, not Ctrl+Left: Qt's ExtendedSelection hardwires Ctrl = toggle in the
    // hierarchy tree, so leaving pan on Ctrl would make the modifier mean two
    // different things depending on which panel the cursor was over. Alt+drag to
    // navigate is the Unity/Blender/Maya convention. Middle-mouse pan is unchanged.
    //
    // Hoisted because three handlers read this and they must not drift apart.
    return button == Qt::MiddleButton
        || (button == Qt::LeftButton && (modifiers & Qt::AltModifier));
}

void SceneViewGui::mousePressEvent(QMouseEvent* event)
{
    if (IsPanGesture(event->button(), event->modifiers()))
    {
        isPanning = true;
        lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        return;
    }

    if (event->button() == Qt::LeftButton)
    {
        // Don't disturb the selection if GizmosManager is using a handle
        if (GizmosManager::Get()->WantsCaptureMouse())
            return;

        makeCurrent();  // Ensure OpenGL context is active for ReadPixel

        float dpi = devicePixelRatioF();
        int fbX = static_cast<int>(event->pos().x() * dpi);
        int fbY = static_cast<int>((height() - event->pos().y()) * dpi);  // Flip Y for OpenGL

        std::string objectId = editorView->Pick(fbX, fbY);

        auto* selMgr = Engine::Get()->GetActiveContainer()->FindSystem<SelectionManager>();
        const bool additive =
            event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);

        if (!objectId.empty()) {
            if (additive) selMgr->ToggleSelection(objectId);
            else          selMgr->Select(objectId);
        } else if (!additive) {
            // Only a plain click on empty space clears. An additive click that
            // happens to miss must leave the selection alone, or building one up
            // becomes an exercise in never missing.
            selMgr->ClearSelection();
        }
    }
}


void SceneViewGui::mouseReleaseEvent(QMouseEvent* event)
{
    if (IsPanGesture(event->button(), event->modifiers()))
    {
        isPanning = false;
        setCursor(Qt::ArrowCursor);
    }
}


void SceneViewGui::mouseMoveEvent(QMouseEvent* e)
{
    Engine* engine = Engine::Get();
    InputManager* inputManager = engine->GetActiveContainer()->FindSystem<InputManager>();

    // Mirrors IsPanGesture, but tested against held buttons rather than the one
    // button that triggered the event.
    bool leftDragPan = (e->buttons() & Qt::LeftButton) && (e->modifiers() & Qt::AltModifier);
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
