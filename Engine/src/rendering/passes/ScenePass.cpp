#include "engine/rendering/passes/ScenePass.hpp"
#include <algorithm>
#include <vector>

void ScenePass::Init(){
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

void ScenePass::Resize(int width, int height)
{
    viewportWidth = width;
    viewportHeight = height;
}

void ScenePass::Execute(RenderCamera* camera, Scene* scene)
{
    const auto& objects = scene->GetAllGameObjects();
    float elapsedTime = timeManager ? timeManager->ElapsedTime() : 0.0f;

    struct DrawCall
    {
        Transform*      transform;
        SpriteRenderer* renderer;
        Material*       mat;
        Shader*         shader;
        int             layerPriority;
        int             sortingOrder;
    };

    std::vector<DrawCall> drawCalls;
    drawCalls.reserve(objects.size());

    for (auto* obj : objects)
    {
        if (!obj || !obj->GetActive()) continue;

        Transform* transform = obj->GetComponent<Transform>();
        SpriteRenderer* renderer = obj->GetComponent<SpriteRenderer>();

        if (!transform)
        {
            Console::Alert("No Loaded Transform");
            continue;
        }
        if (!renderer) continue;
        if (!renderer->GetEnabled() || !renderer->GetVisible()) continue;

        Material* mat = renderer->GetMaterial();
        if (!mat)
        {
            mat = SharedResources::Get().GetMaterialByName("default");
            Console::Alert("Assigning Default Material to " + transform->GetGameObject()->GetName());
        }
        if (!mat || !mat->GetShader()) continue;

        drawCalls.push_back({ transform, renderer, mat, mat->GetShader(),
            layerManager ? layerManager->GetPriority(renderer->GetSortingLayer()) : 0,
            renderer->GetSortingOrder() });
    }

    std::sort(drawCalls.begin(), drawCalls.end(), [](const DrawCall& a, const DrawCall& b)
    {
        if (a.layerPriority != b.layerPriority) return a.layerPriority < b.layerPriority;
        if (a.sortingOrder  != b.sortingOrder)  return a.sortingOrder  < b.sortingOrder;
        return a.shader->GetProgramID() < b.shader->GetProgramID();
    });

    glad_glBindVertexArray(vao);

    const glm::mat4& viewMatrix = camera->GetViewMatrix();
    const glm::mat4& projMatrix = camera->GetProjectionMatrix();
    GLuint lastProgramID = 0;

    for (const DrawCall& dc : drawCalls)
    {
        if (dc.shader->GetProgramID() != lastProgramID)
        {
            dc.shader->Bind();
            dc.shader->SetMat4("uView", viewMatrix);
            dc.shader->SetMat4("uProj", projMatrix);
            lastProgramID = dc.shader->GetProgramID();
        }

        dc.shader->SetMat4("uModel", dc.transform->GetWorldMatrix());
        dc.mat->ApplyUniforms();
        dc.renderer->OverrideUniforms();

        glad_glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    glad_glBindVertexArray(0);
    glad_glUseProgram(0);
}

void ScenePass::Shutdown()
{
    if (vbo) glad_glDeleteBuffers(1, &vbo);
    if (vao) glad_glDeleteVertexArrays(1, &vao);
}
