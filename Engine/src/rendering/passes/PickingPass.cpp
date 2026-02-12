#include "engine/rendering/passes/PickingPass.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include "engine/components/Transform.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/rendering/core/SharedResources.hpp"
#include "engine/rendering/core/Material.hpp"
#include "engine/core/Scene.hpp"
#include "engine/debug/Console.hpp"
#include "engine/utils/EngineUtils.hpp"
#include <iostream>

void PickingPass::Init()
{
    // Create FBO
    glad_glGenFramebuffers(1, &fbo);
    glad_glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Create Picking Texture (Integer format)
    glad_glGenTextures(1, &pickingTexture);
    glad_glBindTexture(GL_TEXTURE_2D, pickingTexture);
    // R32UI for unsigned integer 32-bit
    glad_glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, 1, 1, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glad_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pickingTexture, 0);

    // Create Depth Texture
    glad_glGenTextures(1, &depthTexture);
    glad_glBindTexture(GL_TEXTURE_2D, depthTexture);
    glad_glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 1, 1, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glad_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);

    // Check status
    if (glad_glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "Picking FBO not complete!" << std::endl;

    glad_glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Create Quad
    float quadVerts[] = {
        // pos      // uv
        -0.5f, -0.5f, 0.0f, 0.0f,
         0.5f, -0.5f, 1.0f, 0.0f,
         0.5f,  0.5f, 1.0f, 1.0f,

        -0.5f, -0.5f, 0.0f, 0.0f,
         0.5f,  0.5f, 1.0f, 1.0f,
        -0.5f,  0.5f, 0.0f, 1.0f
    };

    glad_glGenVertexArrays(1, &vao);
    glad_glGenBuffers(1, &vbo);
    glad_glBindVertexArray(vao);
    glad_glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glad_glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    glad_glEnableVertexAttribArray(0); 
    glad_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glad_glEnableVertexAttribArray(1); 
    glad_glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glad_glBindVertexArray(0);

    // Get Shader
    shader = SharedResources::Get().GetShaderByName("picking");
}

void PickingPass::Resize(int width, int height)
{
    if (width <= 0 || height <= 0) return;
    viewportWidth = width;
    viewportHeight = height;

    glad_glBindTexture(GL_TEXTURE_2D, pickingTexture);
    glad_glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, width, height, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);

    glad_glBindTexture(GL_TEXTURE_2D, depthTexture);
    glad_glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
}

void PickingPass::Execute(RenderCamera* camera, Scene* scene)
{

    glad_glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    
    // Validate framebuffer
    GLenum status = glad_glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        return;
    }
    
    glad_glViewport(0, 0, viewportWidth, viewportHeight);
    
    // Clear to 0 (no object)
    uint32_t clearColor = 0;
    glad_glClearBufferuiv(GL_COLOR, 0, &clearColor); 
    glad_glClear(GL_DEPTH_BUFFER_BIT);
    
    // Disable blending for picking
    glad_glDisable(GL_BLEND);
    glad_glEnable(GL_DEPTH_TEST);
    

    shader->Bind();
    shader->SetMat4("uView", camera->GetViewMatrix());
    shader->SetMat4("uProj", camera->GetProjectionMatrix());

    glad_glBindVertexArray(vao);

    const auto& objects = scene->GetAllGameObjects();
    for(auto* obj : objects) {
        if(!obj) continue;
        
        SpriteRenderer* spr = obj->GetComponent<SpriteRenderer>();
        if(!spr || !spr->GetVisible()) continue;
        
        Transform* transform = obj->GetComponent<Transform>();
        if(!transform) continue;

        // Set ID
        uint32_t pickId = obj->GetPickingID();
        shader->SetInt("uId", pickId);
        shader->SetMat4("uModel", transform->GetWorldMatrix());
        
        Sprite* sprite = spr->GetSprite();
        
        // Bind Sprite Texture
        glActiveTexture(GL_TEXTURE0);
        if(sprite && sprite->GetTexture()) {
            glad_glBindTexture(GL_TEXTURE_2D, sprite->GetTexture()->GetTextureID());
        } else {
             glad_glBindTexture(GL_TEXTURE_2D, 0);
        }
        shader->SetInt("uTexture", 0);

        // UV and Size Logic (Replicating SpriteRenderer behavior)
        if(sprite) {
             // UVs
             glm::vec2 uvScale = sprite->GetUVMax() - sprite->GetUVMin();
             glm::vec2 uvOffset = sprite->GetUVMin();
             if (spr->GetFlipX()) { uvScale.x *= -1.0f; uvOffset.x = sprite->GetUVMax().x; }
             if (spr->GetFlipY()) { uvScale.y *= -1.0f; uvOffset.y = sprite->GetUVMax().y; }
             shader->SetVec2("uUVScale", uvScale);
             shader->SetVec2("uUVOffset", uvOffset);

             // Size & Pivot
             glm::vec2 worldSize = EngineUtils::RenderUtils::PixelsToWorld(sprite->GetPixelSize());
             shader->SetVec2("uSize", worldSize);
             shader->SetVec2("uPivot", sprite->GetPivot());
        } else {
             // Defaults if no sprite
             shader->SetVec2("uUVScale", glm::vec2(1.0f));
             shader->SetVec2("uUVOffset", glm::vec2(0.0f));
             shader->SetVec2("uSize", glm::vec2(1.0f));
             shader->SetVec2("uPivot", glm::vec2(0.5f));
        }

        glad_glDrawArrays(GL_TRIANGLES, 0, 6);
    
    }

    glad_glBindVertexArray(0);
    glad_glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

uint32_t PickingPass::ReadPixel(int x, int y)
{
    std::cout << "ReadPixel at (" << x << ", " << y << ") viewport: " << viewportWidth << "x" << viewportHeight << std::endl;
    
    // Validate coordinates
    if (x < 0 || x >= viewportWidth || y < 0 || y >= viewportHeight) {
        std::cerr << "  ERROR: Coordinates out of bounds!" << std::endl;
        return 0;
    }
    
    glad_glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glad_glReadBuffer(GL_COLOR_ATTACHMENT0);
    
    uint32_t pixel = 0;
    
    glad_glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, &pixel);
    
    std::cout << "  Read ID: " << pixel << std::endl;
    
    glad_glReadBuffer(GL_NONE);
    glad_glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    return pixel;
}

void PickingPass::Shutdown()
{
    glad_glDeleteFramebuffers(1, &fbo);
    glad_glDeleteTextures(1, &pickingTexture);
    glad_glDeleteTextures(1, &depthTexture);
    glad_glDeleteVertexArrays(1, &vao);
    glad_glDeleteBuffers(1, &vbo);
}
