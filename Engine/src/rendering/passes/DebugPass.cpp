#include "engine/rendering/passes/DebugPass.hpp"
#include "engine/components/Transform.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include "engine/components/BoxCollider.hpp"
#include "engine/components/CircleCollider.hpp"
#include "engine/components/CapsuleCollider.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "engine/core/SelectionManager.hpp"
#include "engine/core/Container.hpp"
#include <vector>
#include <cmath>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

using namespace EngineUtils::RenderUtils;
using namespace EngineUtils::MathUtils;

static unsigned int CreateVAO(unsigned int& vbo, const float* data, size_t dataSize)
{
    unsigned int vao;
    glad_glGenVertexArrays(1, &vao);
    glad_glGenBuffers(1, &vbo);

    glad_glBindVertexArray(vao);
    glad_glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glad_glBufferData(GL_ARRAY_BUFFER, dataSize, data, GL_STATIC_DRAW);

    glad_glEnableVertexAttribArray(0);
    glad_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    glad_glEnableVertexAttribArray(1);
    glad_glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glad_glBindVertexArray(0);
    return vao;
}

void DebugPass::Init()
{
    float boxVerts[] = {
        -0.5f, -0.5f,   0.0f, 0.0f,
         0.5f, -0.5f,   0.0f, 0.0f,
         0.5f,  0.5f,   0.0f, 0.0f,
        -0.5f,  0.5f,   0.0f, 0.0f,
    };
    boxVao = CreateVAO(boxVbo, boxVerts, sizeof(boxVerts));

    const int segments = 32;
    std::vector<float> circleVerts;
    for (int i = 0; i < segments; ++i)
    {
        float angle = (2.0f * PI * i) / segments;
        circleVerts.push_back(std::cos(angle)); 
        circleVerts.push_back(std::sin(angle)); 
        circleVerts.push_back(0.0f);           
        circleVerts.push_back(0.0f);            
    }
    circleVertexCount = segments;
    circleVao = CreateVAO(circleVbo, circleVerts.data(), circleVerts.size() * sizeof(float));

    const int semiSegments = 17;
    std::vector<float> capsuleVerts;
    for (int i = 0; i < semiSegments; ++i)
    {
        float angle = (PI * i) / (semiSegments - 1);
        capsuleVerts.push_back(0.0f);        
        capsuleVerts.push_back(0.5f);            
        capsuleVerts.push_back(std::cos(angle));
        capsuleVerts.push_back(std::sin(angle)); 
    }

    for (int i = 0; i < semiSegments; ++i)
    {
        float angle = PI + (PI * i) / (semiSegments - 1);
        capsuleVerts.push_back(0.0f);            
        capsuleVerts.push_back(-0.5f);           
        capsuleVerts.push_back(std::cos(angle)); 
        capsuleVerts.push_back(std::sin(angle)); 
    }

    capsuleVertexCount = semiSegments * 2;
    capsuleVao = CreateVAO(capsuleVbo, capsuleVerts.data(), capsuleVerts.size() * sizeof(float));

    // Line: 2 verts along the X axis. Shader: scaledPos = aPos * size + offset
    // So a line from A to B is encoded as offset=A, size=B-A, model=identity
    float lineVerts[] = {
        0.0f, 0.0f,  0.0f, 0.0f,
        1.0f, 0.0f,  0.0f, 0.0f,
    };
    lineVao = CreateVAO(lineVbo, lineVerts, sizeof(lineVerts));

    // Dynamic polygon VAO – vertices re-uploaded each draw call
    glad_glGenVertexArrays(1, &polyVao);
    glad_glGenBuffers(1, &polyVbo);
    glad_glBindVertexArray(polyVao);
    glad_glBindBuffer(GL_ARRAY_BUFFER, polyVbo);
    glad_glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    glad_glEnableVertexAttribArray(0);
    glad_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glad_glEnableVertexAttribArray(1);
    glad_glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glad_glBindVertexArray(0);

    glad_glGenBuffers(1, &ssbo);

    debugShader = AssetManager::Get().GetShaderByName("debug");
}

void DebugPass::DrawInstanced(unsigned int vao, int vertexCount, GLenum mode,
                              const std::vector<DebugInstanceData>& instances)
{
    if (instances.empty()) return;

    glad_glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glad_glBufferData(GL_SHADER_STORAGE_BUFFER,
                      instances.size() * sizeof(DebugInstanceData),
                      instances.data(), GL_DYNAMIC_DRAW);
    glad_glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);

    glad_glBindVertexArray(vao);
    glad_glDrawArraysInstanced(mode, 0, vertexCount, static_cast<GLsizei>(instances.size()));
}

void DebugPass::Execute(RenderCamera* camera, Scene* scene)
{
    if (!debugShader) return;

    debugShader->Bind();
    debugShader->SetMat4("uView", camera->GetViewMatrix());
    debugShader->SetMat4("uProj", camera->GetProjectionMatrix());

    const auto& objects = scene->GetAllGameObjects();

    std::vector<DebugInstanceData> boxInstances;
    std::vector<DebugInstanceData> circleInstances;
    std::vector<DebugInstanceData> capsuleInstances;

    for (auto* obj : objects)
    {
        if (!obj || !obj->GetActive()) continue;

        Transform* transform = obj->GetComponent<Transform>();
        if (!transform) continue;

        for (BoxCollider* collider : obj->GetComponents<BoxCollider>())
        {
            if (!collider->GetEnabled()) continue;

            DebugInstanceData inst{};
            inst.model = transform->GetWorldMatrix();
            inst.size = collider->GetSize();
            inst.semiSize = glm::vec2(0.0f);
            inst.offset = collider->GetCenter();
            inst.pivot = glm::vec2(0.0f);
            inst.color = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
            boxInstances.push_back(inst);
        }

        for (CircleCollider* collider : obj->GetComponents<CircleCollider>())
        {
            if (!collider->GetEnabled()) continue;

            float radius = collider->GetRadius();
            glm::vec2 worldScale = transform->GetWorldScale();
            // Physics scales a circle's radius uniformly by the largest axis
            // (CircleCollider::CreateShape). A scale-free model (world position +
            // rotation only) plus a uniform scale baked into the size keeps the
            // circle circular instead of skewed by non-uniform object scale.
            float uniformScale = std::max(std::abs(worldScale.x), std::abs(worldScale.y));

            glm::mat4 model = glm::translate(glm::mat4(1.0f),
                                             glm::vec3(transform->GetWorldPosition(), 0.0f));
            model = glm::rotate(model, glm::radians(transform->GetWorldRotation()),
                                glm::vec3(0.0f, 0.0f, 1.0f));

            DebugInstanceData inst{};
            inst.model = model;
            inst.size = glm::vec2(radius * uniformScale);
            inst.semiSize = glm::vec2(0.0f);
            // Center offset is a position, so it scales with both axes to stay put.
            inst.offset = collider->GetCenter() * worldScale;
            inst.pivot = glm::vec2(0.0f);
            inst.color = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
            circleInstances.push_back(inst);
        }

        for (CapsuleCollider* collider : obj->GetComponents<CapsuleCollider>())
        {
            if (!collider->GetEnabled()) continue;

            float radius = collider->GetRadius();
            float height = collider->GetHeight();
            glm::vec2 worldScale = transform->GetWorldScale();
            // Scale uniformly by the largest axis so the capsule isn't skewed by
            // non-uniform object scale. With a scale-free model (world position +
            // rotation) the round caps stay circular without the old sx/sy fudge.
            float uniformScale = std::max(std::abs(worldScale.x), std::abs(worldScale.y));

            glm::mat4 model = glm::translate(glm::mat4(1.0f),
                                             glm::vec3(transform->GetWorldPosition(), 0.0f));
            model = glm::rotate(model, glm::radians(transform->GetWorldRotation()),
                                glm::vec3(0.0f, 0.0f, 1.0f));

            DebugInstanceData inst{};
            inst.model = model;
            inst.size = glm::vec2(0.0f, height * uniformScale);
            inst.semiSize = glm::vec2(radius / 2.0f * uniformScale);
            inst.offset = collider->GetCenter() * worldScale;
            inst.pivot = glm::vec2(0.0f);
            inst.color = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
            capsuleInstances.push_back(inst);
        }
    }
    DrawInstanced(boxVao, 4, GL_LINE_LOOP, boxInstances);
    DrawInstanced(circleVao, circleVertexCount, GL_LINE_LOOP, circleInstances);
    DrawInstanced(capsuleVao, capsuleVertexCount, GL_LINE_LOOP, capsuleInstances);
    {
        auto* debug = DebugDrawManager::Get();

        std::vector<DebugInstanceData> scriptBoxInstances;
        std::vector<DebugInstanceData> scriptCircleInstances;
        std::vector<DebugInstanceData> scriptLineInstances;

        for (const auto& cmd : debug->GetCommands())
        {
            switch (cmd.type)
            {
            case DebugDrawCommand::Type::Point:
            {
                // Rendered as a small circle
                DebugInstanceData inst{};
                inst.model    = glm::mat4(1.0f);
                inst.size     = glm::vec2(cmd.radius, cmd.radius);
                inst.semiSize = glm::vec2(0.0f);
                inst.offset   = cmd.a;
                inst.pivot    = glm::vec2(0.0f);
                inst.color    = cmd.color;
                scriptCircleInstances.push_back(inst);
                break;
            }
            case DebugDrawCommand::Type::Line:
            {
                // Vert 0=(0,0), Vert 1=(1,0). Component-wise aPos*size means Y of size
                // is always zeroed for vert 1. Instead: rotate the model so the local X
                // axis points from A to B, then use length as the X size.
                glm::vec2 dir   = cmd.b - cmd.a;
                float     len   = glm::length(dir);
                float     angle = std::atan2(dir.y, dir.x);

                glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(cmd.a, 0.0f));
                model = glm::rotate(model, angle, glm::vec3(0.0f, 0.0f, 1.0f));

                DebugInstanceData inst{};
                inst.model    = model;
                inst.size     = glm::vec2(len, 0.0f);
                inst.semiSize = glm::vec2(0.0f);
                inst.offset   = glm::vec2(0.0f);
                inst.pivot    = glm::vec2(0.0f);
                inst.color    = cmd.color;
                scriptLineInstances.push_back(inst);
                break;
            }
            case DebugDrawCommand::Type::Box:
            {
                float rad = glm::radians(cmd.rotation);
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(cmd.a, 0.0f));
                model = glm::rotate(model, rad, glm::vec3(0, 0, 1));

                DebugInstanceData inst{};
                inst.model    = model;
                inst.size     = cmd.b;
                inst.semiSize = glm::vec2(0.0f);
                inst.offset   = glm::vec2(0.0f); 
                inst.pivot    = glm::vec2(0.0f);
                inst.color    = cmd.color;
                scriptBoxInstances.push_back(inst);
                break;
            }
            case DebugDrawCommand::Type::Circle:
            {
                DebugInstanceData inst{};
                inst.model    = glm::mat4(1.0f);
                inst.size     = glm::vec2(cmd.radius, cmd.radius);
                inst.semiSize = glm::vec2(0.0f);
                inst.offset   = cmd.a;
                inst.pivot    = glm::vec2(0.0f);
                inst.color    = cmd.color;
                scriptCircleInstances.push_back(inst);
                break;
            }
            case DebugDrawCommand::Type::Polygon:
            {
                GLenum outlineMode = cmd.closed ? GL_LINE_LOOP : GL_LINE_STRIP;
                DrawPolygonDirect(cmd.points, outlineMode, cmd.color);
                if (cmd.filled && cmd.points.size() >= 3)
                {
                    glm::vec4 fillColor = cmd.color;
                    fillColor.a *= 0.5f;
                    DrawPolygonDirect(cmd.points, GL_TRIANGLE_FAN, fillColor);
                }
                break;
            }
            }
        }

        DrawInstanced(boxVao,    4,                  GL_LINE_LOOP, scriptBoxInstances);
        DrawInstanced(circleVao, circleVertexCount,  GL_LINE_LOOP, scriptCircleInstances);
        DrawInstanced(lineVao,   2,                  GL_LINES,     scriptLineInstances);

        debug->Clear();
    }

    glad_glBindVertexArray(0);
}

void DebugPass::DrawPolygonDirect(const std::vector<glm::vec2>& pts, GLenum mode, const glm::vec4& color)
{
    if (pts.empty()) return;

    std::vector<float> verts;
    verts.reserve(pts.size() * 4);
    for (const auto& p : pts)
    {
        verts.push_back(p.x);
        verts.push_back(p.y);
        verts.push_back(0.0f);
        verts.push_back(0.0f);
    }

    glad_glBindVertexArray(polyVao);
    glad_glBindBuffer(GL_ARRAY_BUFFER, polyVbo);
    glad_glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW);
    glad_glBindVertexArray(0);

    // Single identity instance so vertices pass through the shader unchanged
    DebugInstanceData inst{};
    inst.model    = glm::mat4(1.0f);
    inst.size     = glm::vec2(1.0f, 1.0f);
    inst.semiSize = glm::vec2(0.0f);
    inst.offset   = glm::vec2(0.0f);
    inst.pivot    = glm::vec2(0.0f);
    inst.color    = color;

    glad_glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glad_glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(DebugInstanceData), &inst, GL_DYNAMIC_DRAW);
    glad_glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);

    glad_glBindVertexArray(polyVao);
    glad_glDrawArraysInstanced(mode, 0, static_cast<GLsizei>(pts.size()), 1);
    glad_glBindVertexArray(0);
}

void DebugPass::Resize(int width, int height)
{
    viewportWidth = width;
    viewportHeight = height;
}

void DebugPass::Shutdown()
{
    if (boxVbo)     glad_glDeleteBuffers(1, &boxVbo);
    if (boxVao)     glad_glDeleteVertexArrays(1, &boxVao);
    if (circleVbo)  glad_glDeleteBuffers(1, &circleVbo);
    if (circleVao)  glad_glDeleteVertexArrays(1, &circleVao);
    if (capsuleVbo) glad_glDeleteBuffers(1, &capsuleVbo);
    if (capsuleVao) glad_glDeleteVertexArrays(1, &capsuleVao);
    if (lineVbo)    glad_glDeleteBuffers(1, &lineVbo);
    if (lineVao)    glad_glDeleteVertexArrays(1, &lineVao);
    if (polyVbo)    glad_glDeleteBuffers(1, &polyVbo);
    if (polyVao)    glad_glDeleteVertexArrays(1, &polyVao);
    if (ssbo)       glad_glDeleteBuffers(1, &ssbo);
    if (debugShader) delete debugShader;
}
