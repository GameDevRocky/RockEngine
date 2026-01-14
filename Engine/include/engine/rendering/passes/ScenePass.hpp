#pragma once
#include <glad/glad.h>
#include "engine/rendering/passes/RenderPass.hpp"
#include "engine/components/Transform.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include "engine/rendering/core/Material.hpp"
#include "engine/rendering/core/SharedResources.hpp"
#include "engine/debug/Console.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Engine.hpp"

class ScenePass : public RenderPass
{
public:
    ScenePass() = default;

    void Init() override
    {
        // Full-quad setup
        float quadVerts[] =
        {
            // pos      // uv
            -0.5f, -0.5f, 0.0f, 0.0f,
             0.5f, -0.5f, 1.0f, 0.0f,
             0.5f,  0.5f, 1.0f, 1.0f,

            -0.5f, -0.5f, 0.0f, 0.0f,
             0.5f,  0.5f, 1.0f, 1.0f,
            -0.5f,  0.5f, 0.0f, 1.0f
        };
        glad_glEnable(GL_BLEND);
        glad_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


        glad_glGenVertexArrays(1, &vao);
        glad_glGenBuffers(1, &vbo);

        glad_glBindVertexArray(vao);
        glad_glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glad_glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);

        glad_glEnableVertexAttribArray(0); // pos
        glad_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

        glad_glEnableVertexAttribArray(1); // uv
        glad_glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

        glad_glBindVertexArray(0);
    }

    void Resize(int width, int height) override
    {
        viewportWidth = width;
        viewportHeight = height;
    }

    void Execute(RenderCamera& camera, Scene& scene) override
    {
        const auto& objects = scene.GetAllGameObjects();
        glad_glBindVertexArray(vao);

        for (auto* obj : objects)
        {
            if (!obj) continue;
            Transform* transform = obj->GetComponent<Transform>();
            SpriteRenderer* renderer = obj->GetComponent<SpriteRenderer>();
      
            if (!transform || !renderer){
                Console::Alert("No Loaded Transform or Sprite");
                continue;
            }
            if (!renderer->GetVisible()) continue;

            Material* mat = renderer->GetMaterial();
            if (!mat){
                mat = SharedResources::Get().GetMaterialByName("default"); 
                Console::Alert("Assigning Default Material to " + transform->GetGameObject()->GetName());
                }
            if (!mat || !mat->GetShader()) continue; 

            Shader* shader = mat->GetShader();
            shader->Bind();
            shader->SetMat4("uView", camera.GetViewMatrix());
            shader->SetMat4("uProj", camera.GetProjectionMatrix());
            shader->SetMat4("uModel", transform->GetWorldMatrix());
            TimeManager* timeManager = Engine::Get()->GetActiveContainer()->GetTimeManager();
            shader->SetFloat("uTime", timeManager->ElapsedTime());
            mat->ApplyUniforms();
            renderer->OverrideUniforms();

            glad_glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        glad_glBindVertexArray(0);
    }

    void Shutdown() override
    {
        if (vbo) glad_glDeleteBuffers(1, &vbo);
        if (vao) glad_glDeleteVertexArrays(1, &vao);
    }

private:
    unsigned int vao = 0;
    unsigned int vbo = 0;
    int viewportWidth = 0;
    int viewportHeight = 0;
};
