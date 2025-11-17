#include "engine/rendering/pipelines/RenderPipeline.hpp"
#include <glad/glad.h>
#include <iostream>

RenderPipeline::RenderPipeline() {}

RenderPipeline::~RenderPipeline()
{
    Shutdown();
}

void RenderPipeline::AddPass(RenderPass* pass)
{
    if (!pass)
        throw std::runtime_error("RenderPipeline::AddPass - pass is null");

    passes.push_back(pass);
}

void RenderPipeline::Init()
{
    // Create FBO before pass initialization
    CreateOutputFBO(viewportWidth, viewportHeight);

    for (auto* pass : passes)
        pass->Init();
}

void RenderPipeline::Resize(int width, int height)
{
    viewportWidth = width;
    viewportHeight = height;

    CreateOutputFBO(width, height);

    for (auto* pass : passes)
        pass->Resize(width, height);
}

void RenderPipeline::Render(RenderCamera& camera, Scene& scene)
{
    // Bind the pipeline’s FBO so ALL passes render into it
    glBindFramebuffer(GL_FRAMEBUFFER, outputFBO);

    glViewport(0, 0, viewportWidth, viewportHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    for (auto* pass : passes)
        pass->Execute(camera, scene);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderPipeline::Shutdown()
{
    for (auto* pass : passes)
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
