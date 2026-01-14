#pragma once
#include <glad/glad.h>

#include "engine/rendering/passes/RenderPass.hpp"
#include "engine/rendering/cameras/RenderCamera.hpp"
#include "engine/core/Scene.hpp"
#include "engine/rendering/core/Shader.hpp"
#include "engine/core/TimeManager.hpp"
#include "engine/rendering/core/SharedResources.hpp"
#include "engine/debug/Console.hpp"
#include <memory>
#include "Engine.hpp"

class GridPass : public RenderPass
{
public:
    GridPass() = default;
    ~GridPass() override { Shutdown(); }

    void Init() override
    {
        float vertices[] = {
            -1.0f, -1.0f, 0.0f,
             3.0f, -1.0f, 0.0f,
            -1.0f,  3.0f, 0.0f
        };

        glad_glGenVertexArrays(1, &vao);
        glad_glGenBuffers(1, &vbo);

        glad_glBindVertexArray(vao);
        glad_glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glad_glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glad_glEnableVertexAttribArray(0);
        glad_glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glad_glBindVertexArray(0);

        shader = SharedResources::Get().GetShaderByName("grid");
        if (!shader) Console::Alert("Grid shader not Loaded");

    }

    void Shutdown() override
    {
        if (vbo) glad_glDeleteBuffers(1, &vbo);
        if (vao) glad_glDeleteVertexArrays(1, &vao);
        vao = vbo = 0;
        if (shader) { delete shader; shader = nullptr; }
    }

    void Resize(int w, int h) override
    {
        viewportWidth = w;
        viewportHeight = h;
    }

    void Execute(RenderCamera& camera, Scene&) override
    {
        if (!shader || vao == 0)
            return;

        glad_glDisable(GL_DEPTH_TEST);
        glad_glBindVertexArray(vao);

        shader->Bind();
        shader->SetMat4("uView", camera.GetViewMatrix());
        shader->SetMat4("uProj", camera.GetProjectionMatrix());
        TimeManager* timeManager = Engine::Get()->GetActiveContainer()->GetTimeManager();
        shader->SetFloat("uTime", timeManager->ElapsedTime());
        shader->SetFloat("uZoom", camera.GetZoom());

        glad_glDrawArrays(GL_TRIANGLES, 0, 3);
        glad_glBindVertexArray(0);
    }

private:
    GLuint vao = 0;
    GLuint vbo = 0;
    Shader* shader = nullptr;
    int viewportWidth = 1;  
    int viewportHeight = 1;
};
