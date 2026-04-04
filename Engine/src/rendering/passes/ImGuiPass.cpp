#include "engine/rendering/passes/ImGuiPass.hpp"

#include <glad/glad.h>
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "ImGuizmo.h"
#include "engine/core/SelectionManager.hpp"
#include "engine/core/Container.hpp"
#include "engine/components/Transform.hpp"
#include "engine/utils/EngineUtils.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

void ImGuiPass::Init()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.FontGlobalScale = 2.0f;
    
    ImGui::StyleColorsDark();
    
    ImGui_ImplOpenGL3_Init("#version 460");
}

void ImGuiPass::Resize(int width, int height)
{
    m_width = width > 0 ? width : 1;
    m_height = height > 0 ? height : 1;
}

void ImGuiPass::Execute(RenderCamera* camera, Scene* scene)
{
    
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)m_width, (float)m_height);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();

    ImGui::SetNextWindowBgAlpha(0.35f);
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav;

    if (ImGui::Begin("StatsOverlay", nullptr))
    {
        ImGui::Text("Scene View");
        ImGui::Separator();
        ImGui::Text("FPS: %.1f", io.Framerate);
        ImGui::Text("Viewport: %d x %d", m_width, m_height);
        
        // Gizmo operation controls
        ImGui::Separator();
        if (ImGui::RadioButton("Translate", m_currentOperation == ImGuizmo::TRANSLATE))
            m_currentOperation = ImGuizmo::TRANSLATE;
        ImGui::SameLine();
        if (ImGui::RadioButton("Rotate", m_currentOperation == ImGuizmo::ROTATE))
            m_currentOperation = ImGuizmo::ROTATE;
        ImGui::SameLine();
        if (ImGui::RadioButton("Scale", m_currentOperation == ImGuizmo::SCALE))
            m_currentOperation = ImGuizmo::SCALE;
    }
    ImGui::End();

    Container* container = scene->GetContainer();
    if (container) {
        SelectionManager* selectionManager = container->FindSystem<SelectionManager>();
        if (selectionManager && selectionManager->HasSelection()) {
            GameObject* selectedObj = selectionManager->GetGameObject();
            if (selectedObj) {
                Transform* transform = selectedObj->GetComponent<Transform>();
                if (transform) {
                    glm::mat4 view = camera->GetViewMatrix();
                    glm::mat4 proj = camera->GetProjectionMatrix();
                    
                    glm::mat4 objectMatrix = transform->GetWorldMatrix();
                    
                    ImGuizmo::SetOrthographic(true);
                    ImGuizmo::SetRect(0, 0, (float)m_width, (float)m_height);
                    
                    ImGuizmo::OPERATION op;
                    if (m_currentOperation == ImGuizmo::TRANSLATE) {
                        op = static_cast<ImGuizmo::OPERATION>(ImGuizmo::TRANSLATE_X | ImGuizmo::TRANSLATE_Y);
                    } else if (m_currentOperation == ImGuizmo::SCALE) {
                        op = static_cast<ImGuizmo::OPERATION>(ImGuizmo::SCALE_X | ImGuizmo::SCALE_Y);
                    } else {
                        op = ImGuizmo::ROTATE_Z;
                    }
                    float* snapPtr = nullptr;
                    float pixelSnap = EngineUtils::RenderUtils::PixelsToWorld(128.0f);
                    float translateSnap[3] = { pixelSnap, pixelSnap, pixelSnap };
                    float rotateSnap[3] = { 15.0f, 15.0f, 15.0f };
                    float scaleSnap[3] = { 0.1f, 0.1f, 0.1f };
                    
                    if (!io.KeyAlt) {
                        if (m_currentOperation == ImGuizmo::TRANSLATE) {
                            snapPtr = translateSnap;
                        } else if (m_currentOperation == ImGuizmo::ROTATE) {
                            snapPtr = rotateSnap;
                        } else if (m_currentOperation == ImGuizmo::SCALE) {
                            snapPtr = scaleSnap;
                        }
                    }
                    
                    ImGuizmo::Manipulate(
                            glm::value_ptr(view),
                            glm::value_ptr(proj),
                            op,
                            ImGuizmo::LOCAL,
                            glm::value_ptr(objectMatrix),
                            nullptr,
                            snapPtr);
                    
                    if (ImGuizmo::IsUsing())
                    {
                        float matrixTranslation[3], matrixRotation[3], matrixScale[3];
                        ImGuizmo::DecomposeMatrixToComponents(
                            glm::value_ptr(objectMatrix),
                            matrixTranslation,
                            matrixRotation,
                            matrixScale);
                        
                        transform->SetWorldPosition(glm::vec2(matrixTranslation[0], matrixTranslation[1]));
                        transform->SetWorldRotation(matrixRotation[2]);
                        transform->SetWorldScale(glm::vec2(matrixScale[0], matrixScale[1]));
                    }
                }
            }
        }
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiPass::Shutdown()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui::DestroyContext();
}