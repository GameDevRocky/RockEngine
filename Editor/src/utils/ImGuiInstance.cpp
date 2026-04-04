#include "utils/ImGuiInstance.hpp"
#include <functional>
#include "engine/utils/EngineUtils.hpp"


void ImGuiInstance::Init() {
    IMGUI_CHECKVERSION(); 
    context = ImGui::CreateContext();
    ImGui::SetCurrentContext(context);
    
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.FontGlobalScale = 1.0f; 
    std::string fontPath = EngineUtils::GetAssetPath("Domain/lib/assets/fonts/Nunito-VariableFont_wght.ttf");
    ImFont* mainFont = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 32.0f);
    if (!mainFont) {
        io.Fonts->AddFontDefault(); 
    }

    ImGui::StyleColorsDark();
    ImGui_ImplOpenGL3_Init("#version 460");
}

void ImGuiInstance::Render(){
    MakeCurrent();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();

    for (auto& func : drawCalls){
        func();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiInstance::AddDrawCall(const function& cb){
    this->drawCalls.push_back(cb);
}

void ImGuiInstance::MakeCurrent(){
    ImGui::SetCurrentContext(context);
}


void ImGuiInstance::Resize(int width, int height){
    MakeCurrent();
    ImGuiIO& io = ImGui::GetIO();
    this->width = width > 0 ? width : 1;
    this->height = height > 0 ? height : 1;
    io.DisplaySize = ImVec2((float)width, (float)height);
}

void ImGuiInstance::Shutdown(){
    MakeCurrent();

}