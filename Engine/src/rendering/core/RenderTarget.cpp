#include "engine/rendering/core/RenderTarget.hpp"
#include <glad/glad.h>
#include <iostream>

RenderTarget::~RenderTarget()
{
    Destroy();
}

bool RenderTarget::Resize(int newWidth, int newHeight)
{
    if (newWidth <= 0 || newHeight <= 0) return false;
    if (fbo != 0 && newWidth == width && newHeight == height) return true;

    Destroy();

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenTextures(1, &colorTexture);
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, newWidth, newHeight,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);

    glGenRenderbuffers(1, &depthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, depthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, newWidth, newHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthRBO);

    const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    if (!complete)
        std::cerr << "RenderTarget FBO error: incomplete framebuffer!" << std::endl;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (!complete)
    {
        Destroy();
        return false;
    }

    width = newWidth;
    height = newHeight;
    return true;
}

void RenderTarget::Destroy()
{
    if (colorTexture)
    {
        glDeleteTextures(1, &colorTexture);
        colorTexture = 0;
    }
    if (depthRBO)
    {
        glDeleteRenderbuffers(1, &depthRBO);
        depthRBO = 0;
    }
    if (fbo)
    {
        glDeleteFramebuffers(1, &fbo);
        fbo = 0;
    }
    width = 0;
    height = 0;
}
