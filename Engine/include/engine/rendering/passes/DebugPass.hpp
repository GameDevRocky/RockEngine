#pragma once
#include <glad/glad.h>
#include "engine/rendering/passes/RenderPass.hpp"
#include "engine/core/Scene.hpp"
#include <vector>
#include <glm/glm.hpp>

class DebugPass : public RenderPass
{
public:
    DebugPass() = default;

    void Init() override
    {
        // Optionally setup VAO/VBO for debug lines
        glad_glGenVertexArrays(1, &vao);
        glad_glGenBuffers(1, &vbo);
        glad_glBindVertexArray(vao);
        glad_glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glad_glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 2 * 1000, nullptr, GL_DYNAMIC_DRAW); // 1000 max points
        glad_glEnableVertexAttribArray(0);
        glad_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glad_glBindBuffer(GL_ARRAY_BUFFER, 0);
        glad_glBindVertexArray(0);
    }

    void Resize(int width, int height) override
    {
        // Nothing to do for now
    }

    void Execute(RenderCamera& camera, Scene& scene) override
    {
        DrawGrid();
    }

    void Shutdown() override
    {
        if (vbo) glad_glDeleteBuffers(1, &vbo);
        if (vao) glad_glDeleteVertexArrays(1, &vao);
    }

private:
    GLuint vao = 0;
    GLuint vbo = 0;

    void DrawGrid()
    {
        glad_glBindVertexArray(vao);
        std::vector<float> lines;

        const float step = 0.1f; // grid spacing
        const int count = 50;

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

        glad_glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glad_glBufferSubData(GL_ARRAY_BUFFER, 0, lines.size() * sizeof(float), lines.data());
        glad_glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(lines.size() / 2));
        glad_glBindBuffer(GL_ARRAY_BUFFER, 0);
        glad_glBindVertexArray(0);
    }

};
