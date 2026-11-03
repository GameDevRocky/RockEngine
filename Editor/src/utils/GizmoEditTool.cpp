#include "utils/SceneEditController.hpp"
#include "utils/SceneEditTools.hpp"
#include "utils/TilePaintState.hpp"
#include "engine/rendering/core/GizmosManager.hpp"
#include "engine/rendering/core/GridSettings.hpp"
#include "engine/rendering/views/EditorRenderView.hpp"
#include "imgui.h"

namespace {
class GizmoEditTool final : public SceneEditTool {
public:
    bool WantsCaptureMouse() const override {
        return GizmosManager::Get()->GetEditMode() != GizmosManager::EditMode::Tilemap
            && GizmosManager::Get()->WantsCaptureMouse();
    }
    bool PointerPress(const SceneEditContext&, const SceneEditPointerEvent& event) override {
        return event.button == Qt::LeftButton && WantsCaptureMouse();
    }
    bool PointerRelease(const SceneEditContext&, const SceneEditPointerEvent&) override {
        return true;
    }
    bool KeyPress(const SceneEditContext&, int key, Qt::KeyboardModifiers) override {
        return GizmosManager::Get()->GetEditMode() != GizmosManager::EditMode::Tilemap
            && key == Qt::Key_Delete && GizmosManager::Get()->DeleteSelectedChainPoint();
    }
    void Draw(const SceneEditContext& context) override {
        if (!context.view) return;
        GizmosManager* gizmos = GizmosManager::Get();
        gizmos->SetSnapRequested(context.modifiers.testFlag(Qt::ControlModifier)
                                 || GridSettings::Get().IsSnapEnabled());
        gizmos->SetUniformScaleRequested(context.modifiers.testFlag(Qt::AltModifier));
        gizmos->SetChainExtendRequested(context.modifiers.testFlag(Qt::ShiftModifier));
        gizmos->SetChainInsertRequested(context.modifiers.testFlag(Qt::ControlModifier));
        auto* camera = context.view->GetCamera();
        // Painting has input priority. Keep passive gizmo overlays visible, but
        // present a released left button to gizmo handles for this draw so a
        // transform/collider drag cannot start underneath a tile stroke.
        ImGuiIO& io = ImGui::GetIO();
        const bool savedMouseDown = io.MouseDown[0];
        if (gizmos->GetEditMode() == GizmosManager::EditMode::Tilemap)
            io.MouseDown[0] = false;
        gizmos->DrawGizmos(camera->GetViewMatrix(), camera->GetProjectionMatrix(),
                           static_cast<float>(context.width), static_cast<float>(context.height));
        io.MouseDown[0] = savedMouseDown;
    }
};

} // namespace

std::unique_ptr<SceneEditTool> CreateGizmoEditTool() {
    return std::make_unique<GizmoEditTool>();
}
