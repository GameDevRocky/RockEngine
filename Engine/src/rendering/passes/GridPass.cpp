#include "engine/rendering/passes/GridPass.hpp"
#include "engine/utils/EngineUtils.hpp"

void GridPass::Init(){
    float vertices[] = {
            -1.0f, -1.0f, 0.0f,
             3.0f, -1.0f, 0.0f,
            -1.0f,  3.0f, 0.0f
        };

        glad_glGenVertexArrays(1, &vao);
        glad_glGenBuffers(1, &vbo);

        glad_glBindVertexArray(vao);
        glad_glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glad_glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glad_glEnableVertexAttribArray(0);
        glad_glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glad_glBindVertexArray(0);

        shader = AssetManager::Get().GetShaderByName("grid");
        if (!shader) Console::Alert("Grid shader not Loaded");
}


void GridPass::Execute(RenderCamera* camera, Scene* scene){
    if (!shader || vao == 0)
            return;

        glad_glDisable(GL_DEPTH_TEST);
        glad_glBindVertexArray(vao);

        shader->Bind();
        shader->SetMat4("uView", camera->GetViewMatrix());
        shader->SetMat4("uProj", camera->GetProjectionMatrix());
        TimeManager* timeManager = Engine::Get()->GetActiveContainer()->FindSystem<TimeManager>();
        shader->SetFloat("uTime", timeManager->ElapsedTime());
        float zoom = camera->GetZoom();
        float orthoSize = camera->GetOrthoSize();
        // True screen pixels per world unit: the view spans 2*orthoSize/zoom world
        // units over pixelHeight pixels. No fudge factor -- the shader's LOD fades
        // handle density from here.
        float pixelsPerWorldUnit = camera->GetPixelHeight() * zoom / (2.0f * orthoSize);
        shader->SetFloat("uPixelsPerWorldUnit", pixelsPerWorldUnit);
        shader->SetFloat("uBaseSpacing", EngineUtils::RenderUtils::GridCellPixels);

        glad_glDrawArrays(GL_TRIANGLES, 0, 3);
        glad_glBindVertexArray(0);
}

void GridPass::Shutdown(){
    if (vbo) glad_glDeleteBuffers(1, &vbo);
        if (vao) glad_glDeleteVertexArrays(1, &vao);
        vao = vbo = 0;
        // shader is borrowed from AssetManager -- do NOT delete it (see RenderPass.hpp).
        shader = nullptr;
}
