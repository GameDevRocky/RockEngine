#pragma once
#include "engine/rendering/passes/RenderPass.hpp"
#include "engine/components/Transform.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include "engine/rendering/core/Material.hpp"
#include "engine/rendering/core/SharedResources.hpp"
#include "engine/debug/Console.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glad/glad.h>

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

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0); // pos
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

        glEnableVertexAttribArray(1); // uv
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

        glBindVertexArray(0);
    }

    void Resize(int width, int height) override
    {
        viewportWidth = width;
        viewportHeight = height;
    }

    void Execute(RenderCamera& camera, Scene& scene) override
    {
        const auto& objects = scene.GetGameObjects();
        glBindVertexArray(vao);

        for (auto* obj : objects)
        {
            if (!obj) continue;
            
            Transform* transform = obj->GetComponent<Transform>();
            SpriteRenderer* sprite = obj->GetComponent<SpriteRenderer>();
            
            if (!transform || !sprite) continue;
            
            Material* mat = sprite->GetMaterial();
            if (!mat) mat = SharedResources::Get().GetMaterialByName("default");
            if (!mat->GetShader()) Console::Comment("No Default Mat Shader");
            if (!mat || !mat->GetShader()) continue;            
            Shader* shader = mat->GetShader();
            Texture2D* texture = mat->GetTexture();
            shader->Bind();
            texture->Bind();


            // Camera matrices
            mat->SetMat4("uView", camera.GetViewMatrix());
            mat->SetMat4("uProj", camera.GetProjectionMatrix());

            // Model matrix
            glm::mat4 model(1.0f);
            model = glm::translate(model, glm::vec3(transform->position, 0.0f));
            model = glm::rotate(model, glm::radians(transform->rotation), glm::vec3(0, 0, 1));
            model = glm::scale(model, glm::vec3(transform->scale, 1.0f));
            mat->SetMat4("uModel", model);

            // Sprite properties
            mat->SetVec4("uColor", {1,1,1,1});

            // Apply all uniforms
            mat->ApplyUniforms();

            // Draw quad
            glDrawArrays(GL_TRIANGLES, 0, 6);

            shader->Unbind();
        }

        glBindVertexArray(0);
    }

    void Shutdown() override
    {
        if (vbo) glDeleteBuffers(1, &vbo);
        if (vao) glDeleteVertexArrays(1, &vao);
    }

private:
    unsigned int vao = 0;
    unsigned int vbo = 0;
    int viewportWidth = 0;
    int viewportHeight = 0;
};
