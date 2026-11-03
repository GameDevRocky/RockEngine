#include "utils/SceneEditController.hpp"
#include "utils/SceneEditTools.hpp"
#include "utils/ImGuiInstance.hpp"
#include "engine/rendering/views/EditorRenderView.hpp"
#include "engine/rendering/core/GizmosManager.hpp"
#include <QEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <algorithm>
#include <utility>
#include "imgui.h"

glm::vec2 SceneEditContext::ScreenToWorld(const QPoint& point) const {
    if (!view) return {};
    return view->ScreenToWorld({point.x() * dpi, point.y() * dpi});
}

SceneEditController::SceneEditController() {
    RegisterTool(CreateGizmoEditTool());
    RegisterTool(CreateTilemapPaintTool());
}

SceneEditController::SceneEditController(std::vector<std::unique_ptr<SceneEditTool>> initialTools)
    : tools(std::move(initialTools)) {}

SceneEditController::~SceneEditController() = default;

void SceneEditController::RegisterTool(std::unique_ptr<SceneEditTool> tool) {
    if (tool) tools.push_back(std::move(tool));
}

bool SceneEditController::FilterImGuiEvent(ImGuiInstance* imgui, QEvent* event) {
    if (!imgui || !event) return false;
    imgui->MakeCurrent();
    ImGuiIO& io = ImGui::GetIO();
    if (event->type() == QEvent::MouseMove) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        io.MousePos = ImVec2(mouse->pos().x(), mouse->pos().y());
        return false;
    }
    if (event->type() == QEvent::MouseButtonPress) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        io.MousePos = ImVec2(mouse->pos().x(), mouse->pos().y());
        if (mouse->button() == Qt::MiddleButton) return false;
        if (mouse->button() == Qt::LeftButton) io.MouseDown[0] = true;
        if (mouse->button() == Qt::RightButton) io.MouseDown[1] = true;
        if (GizmosManager::Get()->GetEditMode() == GizmosManager::EditMode::Tilemap)
            return false;
        return io.WantCaptureMouse || std::any_of(tools.begin(), tools.end(),
            [](const auto& tool) { return tool->WantsCaptureMouse(); });
    }
    if (event->type() == QEvent::MouseButtonRelease) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        io.MousePos = ImVec2(mouse->pos().x(), mouse->pos().y());
        if (mouse->button() == Qt::MiddleButton) return false;
        if (mouse->button() == Qt::LeftButton) io.MouseDown[0] = false;
        if (mouse->button() == Qt::RightButton) io.MouseDown[1] = false;
        return !captured && io.WantCaptureMouse &&
            GizmosManager::Get()->GetEditMode() != GizmosManager::EditMode::Tilemap;
    }
    if (event->type() == QEvent::Wheel) {
        auto* wheel = static_cast<QWheelEvent*>(event);
        io.MouseWheel += wheel->angleDelta().y() / 120.0f;
    }
    return false;
}

bool SceneEditController::PointerPress(const SceneEditContext& context,
                                       const SceneEditPointerEvent& event) {
    for (auto it = tools.rbegin(); it != tools.rend(); ++it) {
        if ((*it)->PointerPress(context, event)) {
            captured = it->get();
            capturedButton = event.button;
            return true;
        }
    }
    return false;
}

bool SceneEditController::PointerMove(const SceneEditContext& context,
                                      const SceneEditPointerEvent& event) {
    if (captured) return captured->PointerMove(context, event);
    bool handled = false;
    for (auto& tool : tools) handled = tool->PointerMove(context, event) || handled;
    return handled;
}

bool SceneEditController::PointerRelease(const SceneEditContext& context,
                                         const SceneEditPointerEvent& event) {
    if (!captured || event.button != capturedButton) return false;
    SceneEditTool* tool = captured;
    captured = nullptr;
    capturedButton = Qt::NoButton;
    return tool->PointerRelease(context, event);
}

bool SceneEditController::KeyPress(const SceneEditContext& context, int key,
                                   Qt::KeyboardModifiers modifiers) {
    for (auto it = tools.rbegin(); it != tools.rend(); ++it)
        if ((*it)->KeyPress(context, key, modifiers)) return true;
    return false;
}

void SceneEditController::Draw(const SceneEditContext& context) {
    for (auto& tool : tools) tool->Draw(context);
}

void SceneEditController::PointerLeave(const SceneEditContext& context) {
    for (auto& tool : tools) tool->PointerLeave(context);
}

void SceneEditController::Finish(const SceneEditContext& context, ImGuiInstance* imgui) {
    if (imgui) {
        imgui->MakeCurrent();
        ImGuiIO& io = ImGui::GetIO();
        io.MouseDown[0] = false;
        io.MouseDown[1] = false;
    }
    if (captured) {
        captured->Finish(context);
        captured = nullptr;
        capturedButton = Qt::NoButton;
    }
}
