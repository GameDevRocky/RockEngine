#include "engine/rendering/pipelines/RenderPipeline.hpp"
#include <glad/glad.h>
#include <iostream>

RenderPipeline::RenderPipeline() {}

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
    viewportWidth = width;
    viewportHeight = height;

    CreateOutputFBO(width, height);

    for (auto* pass : setupPasses)
        pass->Resize(width, height);
    for (auto* pass : scenePasses)
        pass->Resize(width, height);
    for (auto* pass : finalizePasses)
        pass->Resize(width, height);
}

void RenderPipeline::Render(RenderCamera* camera, std::vector<Scene*> scenes)
{

    GLint prevFBO = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, outputFBO);

    for (auto* pass : setupPasses)
        pass->Execute(camera, nullptr);

    for (auto& scene : scenes){
        for (auto* pass : scenePasses){
            pass->Execute(camera, scene);
        }
    }

    for (auto* pass : finalizePasses)
        pass->Execute(camera, nullptr);
    
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prevFBO);
}

void RenderPipeline::Shutdown()
{
    for (auto* pass : setupPasses)
        pass->Shutdown();
    for (auto* pass : scenePasses)
        pass->Shutdown();
    for (auto* pass : finalizePasses)
        pass->Shutdown();
 
    DestroyOutputFBO();
}

void RenderPipeline::CreateOutputFBO(int width, int height)
{
    DestroyOutputFBO();

    // FBO
    glGenFramebuffers(1, &outputFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, outputFBO);

    // Texture
    glGenTextures(1, &outputTexture);
    glBindTexture(GL_TEXTURE_2D, outputTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outputTexture, 0);

    // Depth/stencil
    glGenRenderbuffers(1, &outputRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, outputRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, outputRBO);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "RenderPipeline FBO error: incomplete framebuffer!" << std::endl;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderPipeline::DestroyOutputFBO()
{
    if (outputTexture)
    {
        glDeleteTextures(1, &outputTexture);
        outputTexture = 0;
    }
    if (outputRBO)
    {
        glDeleteRenderbuffers(1, &outputRBO);
        outputRBO = 0;
    }
    if (outputFBO)
    {
        glDeleteFramebuffers(1, &outputFBO);
        outputFBO = 0;
    }
}
