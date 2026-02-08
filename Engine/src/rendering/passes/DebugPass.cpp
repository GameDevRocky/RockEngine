#include "engine/rendering/passes/DebugPass.hpp"
#include "engine/components/Transform.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include "engine/components/BoxCollider.hpp"
#include "engine/components/CircleCollider.hpp"
#include "engine/components/CapsuleCollider.hpp"
#include "engine/rendering/core/SharedResources.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "Engine.hpp"
#include <vector>
#include <cmath>

using namespace EngineUtils::RenderUtils;
using namespace EngineUtils::MathUtils;

void DebugPass::Init(){

    float lineVerts[] = {
        -0.5f, -0.5f,  
         0.5f, -0.5f, 
         0.5f,  0.5f, 
        -0.5f,  0.5f  
    };

    glad_glGenVertexArrays(1, &vao);
    glad_glGenBuffers(1, &vbo);

    glad_glBindVertexArray(vao);
    glad_glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glad_glBufferData(GL_ARRAY_BUFFER, sizeof(lineVerts), lineVerts, GL_STATIC_DRAW);

    glad_glEnableVertexAttribArray(0); 
    glad_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    glad_glBindVertexArray(0);

    // Create circle vertices
    const int segments = 32;
    std::vector<float> circleVerts;
    for (int i = 0; i < segments; ++i) {
        float angle = (2.0f * PI * i) / segments;
        circleVerts.push_back(cos(angle));
        circleVerts.push_back(sin(angle));
    }

    glad_glGenVertexArrays(1, &circleVao);
    glad_glGenBuffers(1, &circleVbo);

    glad_glBindVertexArray(circleVao);
    glad_glBindBuffer(GL_ARRAY_BUFFER, circleVbo);
    glad_glBufferData(GL_ARRAY_BUFFER, circleVerts.size() * sizeof(float), circleVerts.data(), GL_STATIC_DRAW);

    glad_glEnableVertexAttribArray(0);
    glad_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    glad_glBindVertexArray(0);

    // Create capsule line vertices (two vertical lines)
    float capsuleLineVerts[] = {
        // Left vertical line
        -0.5f, -0.5f,
        -0.5f,  0.5f,
        // Right vertical line
         0.5f, -0.5f,
         0.5f,  0.5f
    };

    glad_glGenVertexArrays(1, &capsuleVao);
    glad_glGenBuffers(1, &capsuleVbo);

    glad_glBindVertexArray(capsuleVao);
    glad_glBindBuffer(GL_ARRAY_BUFFER, capsuleVbo);
    glad_glBufferData(GL_ARRAY_BUFFER, sizeof(capsuleLineVerts), capsuleLineVerts, GL_STATIC_DRAW);

    glad_glEnableVertexAttribArray(0);
    glad_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    glad_glBindVertexArray(0);

    // Create semicircle vertices for capsule top (0 to PI)
    const int semiSegments = 17; // 17 points for a semicircle (including endpoints)
    std::vector<float> topSemiVerts;
    for (int i = 0; i < semiSegments; ++i) {
        float angle = (PI* i) / (semiSegments - 1); // 0 to PI
        topSemiVerts.push_back(cos(angle));
        topSemiVerts.push_back(sin(angle));
    }

    glad_glGenVertexArrays(1, &topSemiVao);
    glad_glGenBuffers(1, &topSemiVbo);

    glad_glBindVertexArray(topSemiVao);
    glad_glBindBuffer(GL_ARRAY_BUFFER, topSemiVbo);
    glad_glBufferData(GL_ARRAY_BUFFER, topSemiVerts.size() * sizeof(float), topSemiVerts.data(), GL_STATIC_DRAW);

    glad_glEnableVertexAttribArray(0);
    glad_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    glad_glBindVertexArray(0);

    // Create semicircle vertices for capsule bottom (PI to 2*PI)
    std::vector<float> bottomSemiVerts;
    for (int i = 0; i < semiSegments; ++i) {
        float angle = PI + (PI * i) / (semiSegments - 1); // PI to 2*PI
        bottomSemiVerts.push_back(cos(angle));
        bottomSemiVerts.push_back(sin(angle));
    }

    glad_glGenVertexArrays(1, &bottomSemiVao);
    glad_glGenBuffers(1, &bottomSemiVbo);

    glad_glBindVertexArray(bottomSemiVao);
    glad_glBindBuffer(GL_ARRAY_BUFFER, bottomSemiVbo);
    glad_glBufferData(GL_ARRAY_BUFFER, bottomSemiVerts.size() * sizeof(float), bottomSemiVerts.data(), GL_STATIC_DRAW);

    glad_glEnableVertexAttribArray(0);
    glad_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    glad_glBindVertexArray(0);

    // Load debug shader
    debugShader = SharedResources::Get().GetShaderByName("debug");
}

void DebugPass::Execute(RenderCamera* camera, Scene* scene){
    if (!debugShader) return;

    const auto& objects = scene->GetAllGameObjects();
    glad_glBindVertexArray(vao);

    debugShader->Bind();
    debugShader->SetMat4("uView", camera->GetViewMatrix());
    debugShader->SetMat4("uProj", camera->GetProjectionMatrix());

    for (auto* obj : objects)
    {
        if (!obj) continue;
        Transform* transform = obj->GetComponent<Transform>();
        BoxCollider* collider = obj->GetComponent<BoxCollider>();

        if (!transform || !collider) continue;

        glm::vec2 size = collider->GetSize();
        glm::vec2 center = collider->GetCenter();

        debugShader->SetMat4("uModel", transform->GetWorldMatrix());
        debugShader->SetVec2("uSize", size);
        debugShader->SetVec2("uPivot", glm::vec2(0.0f, 0.0f));
        debugShader->SetVec2("uOffset", center);
        debugShader->SetVec4("uColor", glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));

        glad_glDrawArrays(GL_LINE_LOOP, 0, 4);
    }

    // Render circles for CircleColliders
    glad_glBindVertexArray(circleVao);
    for (auto* obj : objects)
    {
        if (!obj) continue;
        Transform* transform = obj->GetComponent<Transform>();
        CircleCollider* collider = obj->GetComponent<CircleCollider>();

        if (!transform || !collider) continue;

        float radius = collider->GetRadius();
        glm::vec2 center = collider->GetCenter();

        debugShader->SetMat4("uModel", transform->GetWorldMatrix());
        debugShader->SetVec2("uSize", glm::vec2(radius, radius));
        debugShader->SetVec2("uPivot", glm::vec2(0.0f, 0.0f));
        debugShader->SetVec2("uOffset", center);
        debugShader->SetVec4("uColor", glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));

        glad_glDrawArrays(GL_LINE_LOOP, 0, 32);
    }

    // Render capsules for CapsuleColliders (using capsule lines + circles)
    for (auto* obj : objects)
    {
        if (!obj) continue;
        Transform* transform = obj->GetComponent<Transform>();
        CapsuleCollider* collider = obj->GetComponent<CapsuleCollider>();

        if (!transform || !collider) continue;

        float radius = collider->GetRadius();
        float height = collider->GetHeight();
        glm::vec2 center = collider->GetCenter();
        
        // Calculate dimensions
        float boxHeight = height;
        float boxWidth = radius;
        
        // Draw middle vertical lines (if there's a straight section)
        if (boxHeight > 0.0f) {
            glad_glBindVertexArray(capsuleVao);
            debugShader->SetMat4("uModel", transform->GetWorldMatrix());
            debugShader->SetVec2("uSize", glm::vec2(boxWidth, boxHeight));
            debugShader->SetVec2("uPivot", glm::vec2(0.0f, 0.0f));
            debugShader->SetVec2("uOffset", center);
            debugShader->SetVec4("uColor", glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
            glad_glDrawArrays(GL_LINES, 0, 4); // Draw both vertical lines
        }
        
        // Draw top semicircle
        glad_glBindVertexArray(topSemiVao);
        float topOffset = (height / 2.0f);
        debugShader->SetMat4("uModel", transform->GetWorldMatrix());
        debugShader->SetVec2("uSize", glm::vec2(radius/2, radius/2));
        debugShader->SetVec2("uPivot", glm::vec2(0.0f, 0.0f));
        debugShader->SetVec2("uOffset", center + glm::vec2(0.0f, topOffset));
        debugShader->SetVec4("uColor", glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
        glad_glDrawArrays(GL_LINE_STRIP, 0, 17);
        
        // Draw bottom semicircle
        glad_glBindVertexArray(bottomSemiVao);
        float bottomOffset = -(height / 2.0f);
        debugShader->SetVec2("uOffset", center + glm::vec2(0.0f, bottomOffset));
        glad_glDrawArrays(GL_LINE_STRIP, 0, 17);
    }

    glad_glBindVertexArray(0);
}

void DebugPass::Resize(int width, int height){
    viewportWidth = width;
    viewportHeight = height;
}

void DebugPass::Shutdown(){
    if (vbo) glad_glDeleteBuffers(1, &vbo);
    if (vao) glad_glDeleteVertexArrays(1, &vao);
    if (circleVbo) glad_glDeleteBuffers(1, &circleVbo);
    if (circleVao) glad_glDeleteVertexArrays(1, &circleVao);
    if (capsuleVbo) glad_glDeleteBuffers(1, &capsuleVbo);
    if (capsuleVao) glad_glDeleteVertexArrays(1, &capsuleVao);
    if (topSemiVbo) glad_glDeleteBuffers(1, &topSemiVbo);
    if (topSemiVao) glad_glDeleteVertexArrays(1, &topSemiVao);
    if (bottomSemiVbo) glad_glDeleteBuffers(1, &bottomSemiVbo);
    if (bottomSemiVao) glad_glDeleteVertexArrays(1, &bottomSemiVao);
    if (debugShader) delete debugShader;
}