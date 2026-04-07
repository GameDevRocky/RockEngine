#pragma once
#include <glad/glad.h>
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "ImGuizmo.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <functional>
#include <vector>
#include <iostream>

class ImGuiInstance{
    using function = std::function<void()>;
    public:
    ImGuiInstance() = default;

    void Init();
    void Render();
    void Resize(int width, int height, float dpiScale);
    void MakeCurrent();
    void AddDrawCall(const function& cb);
    void Shutdown();

    ImGuiContext* context = nullptr;
    int width = 1;
    int height = 1;

    std::vector<function> drawCalls;

};