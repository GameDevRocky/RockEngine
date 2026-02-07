#include "engine/rendering/passes/GridPass.hpp"

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

        shader = SharedResources::Get().GetShaderByName("grid");
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
        shader->SetFloat("uZoom", camera->GetZoom());

        glad_glDrawArrays(GL_TRIANGLES, 0, 3);
        glad_glBindVertexArray(0);
}

void GridPass::Resize(int w, int h){
    viewportWidth = w;
    viewportHeight = h;
}

void GridPass::Shutdown(){
    if (vbo) glad_glDeleteBuffers(1, &vbo);
        if (vao) glad_glDeleteVertexArrays(1, &vao);
        vao = vbo = 0;
        if (shader) delete shader; shader = nullptr; 
}