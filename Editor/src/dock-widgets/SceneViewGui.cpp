#include "dock-widgets/SceneViewGui.hpp"
#include "engine/core/InputManager.hpp"
#include "engine/core/SelectionManager.hpp"
#include <glm/glm.hpp>
#include "engine/rendering/Renderer.hpp"
#include "engine/rendering/core/GizmosManager.hpp"
#include "engine/components/BoxCollider.hpp"
#include "engine/components/CircleCollider.hpp"
#include "engine/components/CapsuleCollider.hpp"
#include "engine/components/Transform.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include "engine/rendering/core/Sprite.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "engine/core/SceneManager.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/rendering/core/Material.hpp"
#include "engine/commands/PropertyCommand.hpp"
#include "engine/debug/Console.hpp"
#include "utils/DragDropMime.hpp"
#include "Engine.hpp"
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QFileInfo>
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
#include <QGuiApplication>
#include <memory>
#include <vector>
#include <cfloat>
#include <algorithm>

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

    // Grow the world-space AABB [mn, mx] to include this object's renderable/collider
    // extent and every descendant. Rotation is ignored (axis-aligned approximation),
    // which is fine for framing. `any` becomes true once a real extent was added.
    void ExpandBounds(GameObject* obj, glm::vec2& mn, glm::vec2& mx, bool& any) {
        if (!obj) return;
        Transform* t = obj->GetTransform();
        if (!t) return;

        const glm::vec2 center = t->GetWorldPosition();
        const glm::vec2 scale  = glm::abs(t->GetWorldScale());
        glm::vec2 halfExtent(0.0f);
        bool found = false;

        if (SpriteRenderer* sr = obj->GetComponent<SpriteRenderer>()) {
            if (Sprite* sp = sr->GetSprite()) {
                glm::vec2 ws = EngineUtils::RenderUtils::PixelsToWorld(sp->GetPixelSize());
                halfExtent = glm::max(halfExtent, 0.5f * ws * scale);
                found = true;
            }
        }
        if (BoxCollider* bc = obj->GetComponent<BoxCollider>()) {
            halfExtent = glm::max(halfExtent, 0.5f * glm::abs(bc->GetSize()) * scale);
            found = true;
        }
        if (CircleCollider* cc = obj->GetComponent<CircleCollider>()) {
            float r = cc->GetRadius() * std::max(scale.x, scale.y);
            halfExtent = glm::max(halfExtent, glm::vec2(r));
            found = true;
        }

        if (found) {
            mn = glm::min(mn, center - halfExtent);
            mx = glm::max(mx, center + halfExtent);
            any = true;
        } else {
            // No renderable extent -- still include the object's origin so an empty
            // object at least contributes its position to the framing.
            mn = glm::min(mn, center);
            mx = glm::max(mx, center);
        }

        for (Transform* child : t->GetChildren())
            if (child) ExpandBounds(child->GetGameObject(), mn, mx, any);
    }




SceneViewGui::SceneViewGui(QWidget* parent)
    : ViewportWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAcceptDrops(true);   // accept .scene files dropped from the Folder view
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

    // Ctrl+F: frame the selection -- centre the editor camera on the selected
    // object(s) and adjust zoom so they fill the view regardless of current zoom.
    if (event->key() == Qt::Key_F && (event->modifiers() & Qt::ControlModifier)) {
        FrameSelection();
        return;
    }

    inputManager->SetKeyState(event->key(), true);
}

void SceneViewGui::FrameSelection() {
    if (!editorView) return;
    auto* container = Engine::Get()->GetActiveContainer();
    if (!container) return;
    auto* selMgr = container->FindSystem<SelectionManager>();
    auto* registry = container->FindSystem<Registry>();
    if (!selMgr || !registry || !selMgr->HasSelection()) return;

    glm::vec2 mn(FLT_MAX), mx(-FLT_MAX);
    bool any = false;
    for (const std::string& id : selMgr->GetSelectedRoots())
        if (GameObject* go = registry->Find<GameObject>(id))
            ExpandBounds(go, mn, mx, any);

    if (mx.x < mn.x) return;   // nothing captured

    const glm::vec2 center = 0.5f * (mn + mx);
    // Floor the framed size so empty/point objects don't zoom to infinity.
    const float kMinSpan = 2.0f * EngineUtils::RenderUtils::GridCellPixels;
    const glm::vec2 size = glm::max(mx - mn, glm::vec2(kMinSpan));

    EditorCamera* cam = editorView->GetEditorCamera();
    const float fill  = 0.6f;   // fraction of the view the object should fill
    const float aspect = static_cast<float>(cam->GetPixelWidth())
                       / std::max(1, cam->GetPixelHeight());
    // Half-view-height (world) needed to fit each axis with padding.
    const float reqHalfH = std::max((0.5f * size.y) / fill,
                                    (0.5f * size.x) / (fill * aspect));
    // orthoSize/zoom == half-view-height, so zoom = orthoSize / reqHalfH.
    float zoom = cam->GetOrthoSize() / std::max(reqHalfH, 1e-3f);
    zoom = glm::clamp(zoom, cam->GetMinZoom(), cam->GetMaxZoom());

    cam->SetPosition(center);
    cam->SetZoom(zoom);
    update();   // request a repaint in case this view isn't the frame driver
}

void SceneViewGui::keyReleaseEvent(QKeyEvent* event) {
    Engine* engine = Engine::Get();
    InputManager* inputManager = engine->GetActiveContainer()->FindSystem<InputManager>();
    inputManager->SetKeyState(event->key(), false);
}

// ── Scene-file drag & drop ───────────────────────────────────────────────────
// A .scene dragged from the Folder view onto the viewport loads it, same as
// dropping it on the Hierarchy panel (HierarchyGui::dropEvent).
namespace {
    bool DragHasSceneFile(const QMimeData* mime) {
        if (!mime || !mime->hasUrls()) return false;
        for (const QUrl& url : mime->urls()) {
            if (!url.isLocalFile()) continue;
            const QString p = url.toLocalFile();
            if (p.endsWith(".scene", Qt::CaseInsensitive) ||
                p.endsWith(".yml", Qt::CaseInsensitive))
                return true;
        }
        return false;
    }

    // Material id from a dragged .material/.mat file, or "" if the drag carries no
    // (loaded) material. Materials come from the Folder view as file URLs.
    std::string MaterialIdFromMime(const QMimeData* mime) {
        if (!mime || !mime->hasUrls()) return {};
        for (const QUrl& url : mime->urls()) {
            if (!url.isLocalFile()) continue;
            const QString p = url.toLocalFile();
            if (!p.endsWith(".material", Qt::CaseInsensitive) &&
                !p.endsWith(".mat", Qt::CaseInsensitive))
                continue;
            try {
                YAML::Node node = YAML::LoadFile(p.toStdString());
                if (node["id"]) {
                    const std::string id = node["id"].as<std::string>();
                    if (AssetManager::Get().GetMaterial(id)) return id;
                }
            } catch (...) {}
        }
        return {};
    }
}

void SceneViewGui::dragEnterEvent(QDragEnterEvent* event) {
    m_matDragId = MaterialIdFromMime(event->mimeData());
    if (!m_matDragId.empty()) { event->acceptProposedAction(); return; }

    if (event->mimeData()->hasFormat(kSpriteMimeType) || DragHasSceneFile(event->mimeData()))
        event->acceptProposedAction();
    else event->ignore();
}

void SceneViewGui::dragMoveEvent(QDragMoveEvent* event) {
    if (!m_matDragId.empty()) {
        UpdateMaterialPreview(event->position());   // live-preview under the cursor
        event->acceptProposedAction();
        return;
    }
    if (event->mimeData()->hasFormat(kSpriteMimeType) || DragHasSceneFile(event->mimeData()))
        event->acceptProposedAction();
    else event->ignore();
}

void SceneViewGui::dragLeaveEvent(QDragLeaveEvent* event) {
    // Drag cancelled / left the view: undo the preview.
    RestoreMaterialPreview();
    m_matDragId.clear();
    event->accept();
}

void SceneViewGui::dropEvent(QDropEvent* event) {
    // A material dropped onto an object keeps the previewed assignment (with undo).
    if (!m_matDragId.empty()) {
        Container* container = Engine::Get()->GetActiveContainer();
        Registry* registry = container ? container->FindSystem<Registry>() : nullptr;
        GameObject* go = (registry && !m_matPreviewObjId.empty())
                       ? registry->Find<GameObject>(m_matPreviewObjId) : nullptr;
        SpriteRenderer* sr = go ? go->GetComponent<SpriteRenderer>() : nullptr;

        if (sr && m_matPreviewOrigId != m_matDragId) {
            if (auto* undo = container->FindSystem<UndoSystem>())
                undo->Push(std::make_unique<PropertyCommand<SpriteRenderer, std::string>>(
                    sr->GetID(), "material_id", m_matPreviewOrigId, m_matDragId,
                    [](SpriteRenderer* r, const std::string& v) { std::string id = v; r->SetMaterial(id); },
                    "Assign Material"));
        }

        // Keep the preview applied; just drop the tracking (no restore).
        const bool applied = sr != nullptr;
        m_matPreviewObjId.clear();
        m_matPreviewOrigId.clear();
        m_matDragId.clear();
        if (applied) event->acceptProposedAction();
        else         event->ignore();
        return;
    }

    // A sprite dragged from the Folder view's hover column spawns an object.
    if (event->mimeData()->hasFormat(kSpriteMimeType)) {
        const std::string spriteId =
            QString::fromUtf8(event->mimeData()->data(kSpriteMimeType)).toStdString();
        if (SpawnSpriteObject(spriteId, event->position())) event->acceptProposedAction();
        else event->ignore();
        return;
    }

    if (!event->mimeData()->hasUrls()) { event->ignore(); return; }

    SceneManager* sceneManager = Engine::Get()->GetActiveContainer()->FindSystem<SceneManager>();
    if (!sceneManager) { event->ignore(); return; }

    for (const QUrl& url : event->mimeData()->urls()) {
        if (!url.isLocalFile()) continue;
        const QString filePath = url.toLocalFile();
        if (!filePath.endsWith(".scene", Qt::CaseInsensitive) &&
            !filePath.endsWith(".yml", Qt::CaseInsensitive))
            continue;

        const std::string sceneFilePath = filePath.toStdString();
        Console::Comment("Loading scene from: " + sceneFilePath);
        sceneManager->LoadScene(sceneFilePath);
        event->acceptProposedAction();
        return;
    }
    event->ignore();
}

// Create a GameObject (with a SpriteRenderer set to `spriteId`) in the first
// loaded scene, positioned at the drop point. No-op (returns false) when no scene
// is loaded or the sprite id is unknown.
bool SceneViewGui::SpawnSpriteObject(const std::string& spriteId, const QPointF& viewPos) {
    if (spriteId.empty()) return false;

    Sprite* sprite = AssetManager::Get().GetSprite(spriteId);
    if (!sprite) return false;

    Container* container = Engine::Get()->GetActiveContainer();
    if (!container) return false;
    SceneManager* sceneManager = container->FindSystem<SceneManager>();
    if (!sceneManager) return false;

    // Only when a scene is loaded; use the first one.
    Scene* scene = nullptr;
    for (Scene* s : sceneManager->GetScenes()) { scene = s; break; }
    if (!scene) return false;

    auto* obj = new GameObject();
    obj->SetName(sprite->GetName().empty() ? "Sprite" : sprite->GetName());
    scene->AddGameObject(obj);   // synthesises the Transform

    // Place it under the cursor in world space.
    if (Transform* t = obj->GetTransform()) {
        const glm::vec2 fbPos(viewPos.x() * devicePixelRatioF(),
                              viewPos.y() * devicePixelRatioF());
        t->SetPosition(editorView->ScreenToWorld(fbPos));
    }

    // Add the SpriteRenderer (AddComponent runs Init/PostInit) and assign the sprite.
    auto* renderer = new SpriteRenderer();
    obj->AddComponent(renderer);
    std::string idCopy = spriteId;
    renderer->SetSprite(idCopy);

    if (auto* selMgr = container->FindSystem<SelectionManager>())
        selMgr->Select(obj->GetID());

    if (auto* undoSystem = container->FindSystem<UndoSystem>())
        undoSystem->Push(SubtreeCommand::RecordCreated(
            obj, scene->SnapshotSubtree(obj), "Create Sprite Object"));

    return true;
}

std::string SceneViewGui::PickObjectAt(const QPointF& viewPos) {
    makeCurrent();   // Pick reads the picking-pass buffer via GL
    const float dpi = devicePixelRatioF();
    const int fbX = static_cast<int>(viewPos.x() * dpi);
    const int fbY = static_cast<int>((height() - viewPos.y()) * dpi);   // GL flips Y
    return editorView->Pick(fbX, fbY);
}

void SceneViewGui::UpdateMaterialPreview(const QPointF& viewPos) {
    const std::string picked = PickObjectAt(viewPos);
    if (picked == m_matPreviewObjId) return;   // still over the same target

    RestoreMaterialPreview();   // revert whatever we were previewing before

    if (picked.empty()) return;
    Container* container = Engine::Get()->GetActiveContainer();
    Registry* registry = container ? container->FindSystem<Registry>() : nullptr;
    GameObject* go = registry ? registry->Find<GameObject>(picked) : nullptr;
    SpriteRenderer* sr = go ? go->GetComponent<SpriteRenderer>() : nullptr;
    if (!sr) return;   // only objects with a SpriteRenderer take a material

    Material* orig = sr->GetMaterial();
    m_matPreviewObjId = picked;
    m_matPreviewOrigId = orig ? orig->GetID() : "";
    std::string id = m_matDragId;
    sr->SetMaterial(id);
}

void SceneViewGui::RestoreMaterialPreview() {
    if (m_matPreviewObjId.empty()) return;
    Container* container = Engine::Get()->GetActiveContainer();
    Registry* registry = container ? container->FindSystem<Registry>() : nullptr;
    if (registry && !m_matPreviewOrigId.empty()) {
        if (GameObject* go = registry->Find<GameObject>(m_matPreviewObjId))
            if (SpriteRenderer* sr = go->GetComponent<SpriteRenderer>()) {
                std::string id = m_matPreviewOrigId;
                sr->SetMaterial(id);
            }
    }
    m_matPreviewObjId.clear();
    m_matPreviewOrigId.clear();
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
    //
    // Alt+Left pan is disabled for now: Alt is used as the uniform-scale modifier
    // for the gizmos, so it must not also start a camera pan. Middle-mouse pans.
    (void)modifiers;
    return button == Qt::MiddleButton;
    //  || (button == Qt::LeftButton && (modifiers & Qt::AltModifier));
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
    // Alt+Left pan disabled for now (Alt = uniform-scale modifier); middle-mouse pans.
    bool leftDragPan = false;  // (e->buttons() & Qt::LeftButton) && (e->modifiers() & Qt::AltModifier);
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
    // Feed the real keyboard state (ImGui's io.KeyCtrl is cleared every NewFrame
    // because no ImGui keyboard backend is wired up). Ctrl held == snap to grid.
    const Qt::KeyboardModifiers mods = QGuiApplication::queryKeyboardModifiers();
    GizmosManager::Get()->SetSnapRequested(mods.testFlag(Qt::ControlModifier));
    GizmosManager::Get()->SetUniformScaleRequested(mods.testFlag(Qt::AltModifier));
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
