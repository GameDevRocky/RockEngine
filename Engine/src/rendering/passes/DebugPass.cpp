#include "engine/rendering/passes/DebugPass.hpp"
#include "engine/components/Transform.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include "engine/rendering/core/SharedResources.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "Engine.hpp"

using namespace EngineUtils::RenderUtils;

void DebugPass::Init(){
    // Line loop for outline: 4 corners of the quad
    float lineVerts[] = {
        // Corners of the quad (no UVs needed for debug)
        -0.5f, -0.5f,  // bottom-left
         0.5f, -0.5f,  // bottom-right
         0.5f,  0.5f,  // top-right
        -0.5f,  0.5f   // top-left
    };

    glad_glGenVertexArrays(1, &vao);
    glad_glGenBuffers(1, &vbo);

    glad_glBindVertexArray(vao);
    glad_glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glad_glBufferData(GL_ARRAY_BUFFER, sizeof(lineVerts), lineVerts, GL_STATIC_DRAW);

    glad_glEnableVertexAttribArray(0); // position only
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
    debugShader->SetVec4("uColor", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)); // white

    // Render outline for each object with SpriteRenderer
    for (auto* obj : objects)
    {
        if (!obj) continue;
        Transform* transform = obj->GetComponent<Transform>();
        SpriteRenderer* renderer = obj->GetComponent<SpriteRenderer>();

        if (!transform || !renderer) continue;
        if (!renderer->GetVisible()) continue;

        // Get sprite size and pivot for scaling
        Sprite* spr = renderer->GetSprite();
        glm::vec2 size = glm::vec2(1.0f, 1.0f);
        glm::vec2 pivot = glm::vec2(0.5f, 0.5f);
        if (spr) {
            size = PixelsToWorld(spr->GetPixelSize()); // Use same conversion as sprite rendering
            pivot = spr->GetPivot();
        }

        debugShader->SetMat4("uModel", transform->GetWorldMatrix());
        debugShader->SetVec2("uSize", size);
        debugShader->SetVec2("uPivot", pivot);

        // Draw as line loop
        glad_glDrawArrays(GL_LINE_LOOP, 0, 4);
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
    if (debugShader) delete debugShader;
}