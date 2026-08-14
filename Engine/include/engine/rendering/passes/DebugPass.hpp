#pragma once
#include <glad/glad.h>
#include "engine/rendering/passes/RenderPass.hpp"
#include "engine/core/Scene.hpp"
#include "engine/rendering/core/Shader.hpp"
#include "engine/debug/DebugDrawManager.hpp"
#include <vector>
#include <glm/glm.hpp>

struct DebugInstanceData
{
    glm::mat4 model;
    glm::vec2 size;
    glm::vec2 semiSize;
    glm::vec2 offset;
    glm::vec2 pivot;
    glm::vec4 color;
};

class DebugPass : public RenderPass
{
public:
    DebugPass() = default;

    void Init() override;

    void Execute(RenderCamera* camera, Scene* scene) override;

    void Shutdown() override;

private:
    void DrawInstanced(unsigned int vao, int vertexCount, GLenum mode,
                       const std::vector<DebugInstanceData>& instances);
    void DrawPolygonDirect(const std::vector<glm::vec2>& points, GLenum mode, const glm::vec4& color);

    unsigned int boxVao = 0, boxVbo = 0;
    unsigned int circleVao = 0, circleVbo = 0;
    unsigned int lineVao = 0, lineVbo = 0;
    unsigned int polyVao = 0, polyVbo = 0;
    unsigned int ssbo = 0;

    int circleVertexCount = 0;
    Shader* debugShader = nullptr;
};
