#pragma once

#include "engine/rendering/passes/RenderPass.hpp"
#include "imgui.h"
#include "ImGuizmo.h"

class RenderCamera;
class Scene;

class ImGuiPass : public RenderPass
{
public:
    ImGuiPass() = default;
    void Init() override;
    void Resize(int width, int height) override;
    void Execute(RenderCamera* camera, Scene* scene) override;
    void Shutdown() override;

private:
    int m_width = 1;
    int m_height = 1;
    ImGuizmo::OPERATION m_currentOperation = ImGuizmo::TRANSLATE;
};