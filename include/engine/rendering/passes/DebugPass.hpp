#pragma once
#include "engine/rendering/passes/RenderPass.hpp"
#include "engine/core/Scene.hpp"
#include <glad/glad.h>
#include <vector>
#include <glm/glm.hpp>

class DebugPass : public RenderPass
{
public:
    DebugPass() = default;

    void Init() override
    {
        // Optionally setup VAO/VBO for debug lines
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 2 * 1000, nullptr, GL_DYNAMIC_DRAW); // 1000 max points
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    void Resize(int width, int height) override
    {
        // Nothing to do for now
    }

    void Execute(RenderCamera& camera, Scene& scene) override
    {
        // Clear debug overlay (optional, usually transparent)
        // glClearColor(0, 0, 0, 0);
        // glClear(GL_COLOR_BUFFER_BIT);

        // Draw debug info
        DrawGrid();
    }

    void Shutdown() override
    {
        if (vbo) glDeleteBuffers(1, &vbo);
        if (vao) glDeleteVertexArrays(1, &vao);
    }

private:
    GLuint vao = 0;
    GLuint vbo = 0;

    void DrawGrid()
    {
        glBindVertexArray(vao);
        std::vector<float> lines;

        const float step = 0.1f; // grid spacing
        const int count = 20;

        for (int i = -count; i <= count; ++i)
        {
            // Vertical lines
            lines.push_back(i * step);
            lines.push_back(-count * step);

            lines.push_back(i * step);
            lines.push_back(count * step);

            // Horizontal lines
            lines.push_back(-count * step);
            lines.push_back(i * step);

            lines.push_back(count * step);
            lines.push_back(i * step);
        }

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, lines.size() * sizeof(float), lines.data());
        glDrawArrays(GL_LINES, 0, lines.size() / 2);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

};
