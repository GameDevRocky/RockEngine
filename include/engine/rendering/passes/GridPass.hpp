#pragma once

#include "engine/rendering/passes/RenderPass.hpp"
#include "engine/rendering/cameras/RenderCamera.hpp"
#include "engine/core/Scene.hpp"
#include "engine/rendering/core/Shader.hpp"
#include "engine/core/TimeManager.hpp"
#include "engine/rendering/core/SharedResources.hpp"
#include "engine/debug/Console.hpp"
#include <glad/glad.h>
#include <memory>

class GridPass : public RenderPass
{
public:
    GridPass() = default;
    ~GridPass() override { Shutdown(); }

    void Init() override
    {
        // Fullscreen triangle (single triangle covers entire NDC)
        float vertices[] = {
            -1.0f, -1.0f, 0.0f,
             3.0f, -1.0f, 0.0f,
            -1.0f,  3.0f, 0.0f
        };

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glBindVertexArray(0);

        shader = SharedResources::Get().GetShaderByName("grid");
        if (!shader) Console::Alert("Grid shader not Loaded");

    }

    void Shutdown() override
    {
        if (vbo) glDeleteBuffers(1, &vbo);
        if (vao) glDeleteVertexArrays(1, &vao);
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

        glDisable(GL_DEPTH_TEST);
        glBindVertexArray(vao);

        shader->Bind();
        shader->SetMat4("uView", camera.GetViewMatrix());
        shader->SetMat4("uProj", camera.GetProjectionMatrix());
        shader->SetFloat("time", TimeManager::Get().ElapsedTime() );

        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);
    }

private:
    GLuint vao = 0;
    GLuint vbo = 0;
    Shader* shader = nullptr;
    int viewportWidth = 1;  // default width
    int viewportHeight = 1;  // default height
};
