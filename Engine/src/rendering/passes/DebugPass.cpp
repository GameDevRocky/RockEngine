#include "engine/rendering/passes/DebugPass.hpp"
#include "engine/components/Transform.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include "engine/components/BoxCollider.hpp"
#include "engine/components/CircleCollider.hpp"
#include "engine/components/CapsuleCollider.hpp"
#include "engine/rendering/core/SharedResources.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "engine/core/SelectionManager.hpp"
#include "engine/core/Container.hpp"
#include <vector>
#include <cmath>
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

    glad_glGenBuffers(1, &ssbo);

    debugShader = SharedResources::Get().GetShaderByName("debug");
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

        if (BoxCollider* collider = obj->GetComponent<BoxCollider>())
        {
            DebugInstanceData inst{};
            inst.model = transform->GetWorldMatrix();
            inst.size = collider->GetSize();
            inst.semiSize = glm::vec2(0.0f);
            inst.offset = collider->GetCenter();
            inst.pivot = glm::vec2(0.0f);
            inst.color = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
            boxInstances.push_back(inst);
        }

        if (CircleCollider* collider = obj->GetComponent<CircleCollider>())
        {
            float radius = collider->GetRadius();
            DebugInstanceData inst{};
            inst.model = transform->GetWorldMatrix();
            inst.size = glm::vec2(radius, radius);
            inst.semiSize = glm::vec2(0.0f);
            inst.offset = collider->GetCenter();
            inst.pivot = glm::vec2(0.0f);
            inst.color = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
            circleInstances.push_back(inst);
        }

        if (CapsuleCollider* collider = obj->GetComponent<CapsuleCollider>())
        {
            float radius = collider->GetRadius();
            float height = collider->GetHeight();
            glm::vec2 worldScale = glm::abs(transform->GetWorldScale());

            DebugInstanceData inst{};
            inst.model = transform->GetWorldMatrix();
            inst.size = glm::vec2(0.0f, height);
            inst.semiSize = glm::vec2(radius / 2.0f,
                                      radius / 2.0f * worldScale.x / worldScale.y);
            inst.offset = collider->GetCenter();
            inst.pivot = glm::vec2(0.0f);
            inst.color = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
            capsuleInstances.push_back(inst);
        }
    }
    DrawInstanced(boxVao, 4, GL_LINE_LOOP, boxInstances);
    DrawInstanced(circleVao, circleVertexCount, GL_LINE_LOOP, circleInstances);
    DrawInstanced(capsuleVao, capsuleVertexCount, GL_LINE_LOOP, capsuleInstances);

    // DebugDrawManager commands (from Python scripts)
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
            }
        }

        DrawInstanced(boxVao,    4,                  GL_LINE_LOOP, scriptBoxInstances);
        DrawInstanced(circleVao, circleVertexCount,  GL_LINE_LOOP, scriptCircleInstances);
        DrawInstanced(lineVao,   2,                  GL_LINES,     scriptLineInstances);

        debug->Clear();
    }

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
    if (ssbo)       glad_glDeleteBuffers(1, &ssbo);
    if (debugShader) delete debugShader;
}