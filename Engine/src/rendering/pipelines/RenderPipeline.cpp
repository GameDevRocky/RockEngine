#include "engine/rendering/pipelines/RenderPipeline.hpp"
#include <glad/glad.h>
#include <iostream>

RenderPipeline::~RenderPipeline()
{
    Shutdown();
}

void RenderPipeline::AddSetupPass(RenderPass* pass)
{
    if (!pass)
        throw std::runtime_error("RenderPipeline::AddPass - pass is null");
    setupPasses.push_back(pass);

}
void RenderPipeline::AddScenePass(RenderPass* pass)
{
    if (!pass)
        throw std::runtime_error("RenderPipeline::AddPass - pass is null");
    scenePasses.push_back(pass);

}
void RenderPipeline::AddFinalizePass(RenderPass* pass)
{
    if (!pass)
        throw std::runtime_error("RenderPipeline::AddPass - pass is null");
    finalizePasses.push_back(pass);

}

void RenderPipeline::Init()
{
    for (auto* pass : setupPasses)
        pass->Init();
    for (auto* pass : scenePasses)
        pass->Init();
    for (auto* pass : finalizePasses)
        pass->Init();
}

void RenderPipeline::Resize(int width, int height)
{
    outputTarget.Resize(width, height);

    for (auto* pass : setupPasses)
        pass->Resize(width, height);
    for (auto* pass : scenePasses)
        pass->Resize(width, height);
    for (auto* pass : finalizePasses)
        pass->Resize(width, height);
}

void RenderPipeline::Render(RenderCamera* camera, const std::vector<Scene*>& scenes)
{

    GLint prevFBO = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, outputTarget.GetFBO());

    const int targetW = outputTarget.GetWidth();
    const int targetH = outputTarget.GetHeight();

    // Camera's normalized viewport rect narrows drawing to a sub-region of the
    // output target (e.g. split-screen). glClear ignores glViewport but
    // respects glScissor, so scissor must be enabled for ClearPass to clear
    // only the camera's region -- and disabled again after, or later passes
    // (and the widget's subsequent blit) would be silently clipped.
    const glm::vec4& vpRect = camera->GetViewportRect();
    const bool narrowed = vpRect.x != 0.0f || vpRect.y != 0.0f || vpRect.z != 1.0f || vpRect.w != 1.0f;
    GLint vx = static_cast<GLint>(vpRect.x * targetW);
    GLint vy = static_cast<GLint>(vpRect.y * targetH);
    GLsizei vw = static_cast<GLsizei>(glm::max(1.0f, vpRect.z * targetW));
    GLsizei vh = static_cast<GLsizei>(glm::max(1.0f, vpRect.w * targetH));
    glViewport(vx, vy, vw, vh);
    if (narrowed) { glEnable(GL_SCISSOR_TEST); glScissor(vx, vy, vw, vh); }

    for (auto* pass : setupPasses)
        pass->Execute(camera, nullptr);

    for (auto& scene : scenes){
        for (auto* pass : scenePasses){
            pass->Execute(camera, scene);
        }
    }

    for (auto* pass : finalizePasses)
        pass->Execute(camera, nullptr);

    if (narrowed) glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, targetW, targetH);

    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prevFBO);
}

void RenderPipeline::Shutdown()
{
    if (shutdown) return;
    shutdown = true;

    for (auto* pass : setupPasses)    { pass->Shutdown(); delete pass; }
    for (auto* pass : scenePasses)    { pass->Shutdown(); delete pass; }
    for (auto* pass : finalizePasses) { pass->Shutdown(); delete pass; }
    setupPasses.clear();
    scenePasses.clear();
    finalizePasses.clear();

    outputTarget.Destroy();
}
