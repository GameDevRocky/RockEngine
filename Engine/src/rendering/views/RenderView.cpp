#include "engine/rendering/views/RenderView.hpp"
#include "engine/rendering/Renderer.hpp"
#include "engine/debug/FrameProfiler.hpp"
#include <glad/glad.h>

namespace {
    // Largest rect of ratio `aspect` (w/h) that fits within panelW x panelH.
    void FitAspect(float aspect, int panelW, int panelH, int& outW, int& outH)
    {
        const float panelAspect = static_cast<float>(panelW) / static_cast<float>(panelH);
        if (panelAspect > aspect) { outH = panelH; outW = static_cast<int>(panelH * aspect); }
        else                      { outW = panelW; outH = static_cast<int>(panelW / aspect); }
    }
}

RenderView::~RenderView()
{
    delete pipeline;
    delete camera;
}

void RenderView::Resize(int panelPixelW, int panelPixelH)
{
    panelW = glm::max(panelPixelW, 1);
    panelH = glm::max(panelPixelH, 1);
    // Target sizing is (re)applied in Render(), not here: resizeGL can fire
    // before the first UpdateCamera(), so targetAspect may still be a stale
    // default at that point. Deriving the target every frame (and gating on
    // change in ApplyTargetSizing) keeps frame 1 correct and makes a live
    // targetAspect edit take effect immediately.
}

void RenderView::Render()
{
    ROCK_PROFILE_SCOPE("RenderView::Render");
    UpdateCamera();
    ApplyTargetSizing();
    if (!sceneManager) return;
    pipeline->Render(camera, sceneManager->GetScenes());
}

void RenderView::ApplyTargetSizing()
{
    // 1) Render-target pixel size. The display override wins over the camera's
    //    authored target aspect; FromCamera preserves the old behavior.
    int tw, th;
    switch (displayMode)
    {
        case DisplayMode::Free:
            // Fill the panel exactly.
            tw = panelW; th = panelH;
            break;
        case DisplayMode::FixedAspect:
            FitAspect(displayAspect, panelW, panelH, tw, th);
            break;
        case DisplayMode::FixedResolution:
            // Render at an exact internal resolution regardless of panel size;
            // step 2 scales it to fit.
            tw = fixedResW; th = fixedResH;
            break;
        case DisplayMode::FromCamera:
        default:
        {
            const float aspect = camera->GetTargetAspect();
            if (aspect <= 0.0f) { tw = panelW; th = panelH; }   // free
            else FitAspect(aspect, panelW, panelH, tw, th);
            break;
        }
    }
    tw = glm::max(tw, 1);
    th = glm::max(th, 1);

    // Only reallocate the pipeline's FBOs when the target size actually
    // changes -- that's the expensive part.
    if (tw != targetW || th != targetH)
    {
        targetW = tw;
        targetH = th;
        pipeline->Resize(targetW, targetH);
        camera->Resize(targetW, targetH);
    }

    // 2) Where the target lands within the panel. Fit the target's aspect
    //    inside the panel so a fixed-resolution target scales to fit rather
    //    than overflowing at 1:1. For Free / *Aspect / FromCamera the target
    //    already fits the panel, so this is the identity -- presentRect matches
    //    the old (panelW - targetW)/2 centering exactly. Recomputed every frame
    //    from the CURRENT panel size (a cross-dimension letterbox resize keeps
    //    one target dimension constant, which would otherwise leave the blit
    //    rect centered against a stale panel).
    int dw, dh;
    FitAspect(static_cast<float>(targetW) / static_cast<float>(targetH), panelW, panelH, dw, dh);
    dw = glm::max(dw, 1);
    dh = glm::max(dh, 1);
    presentRect = { (panelW - dw) / 2, (panelH - dh) / 2, dw, dh };
}

void RenderView::SetDisplayAspect(float aspect)
{
    displayMode = DisplayMode::FixedAspect;
    displayAspect = glm::max(aspect, 1e-4f);
}

void RenderView::SetDisplayResolution(int w, int h)
{
    displayMode = DisplayMode::FixedResolution;
    fixedResW = glm::max(w, 1);
    fixedResH = glm::max(h, 1);
}

void RenderView::Present(unsigned int targetFBO, int panelPixelW, int panelPixelH)
{
    glBindFramebuffer(GL_FRAMEBUFFER, targetFBO);
    glViewport(0, 0, panelPixelW, panelPixelH);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);   // the letterbox bars
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    Renderer::Get().Blit(GetOutputTexture(), targetFBO,
                          presentRect.x, presentRect.y, presentRect.z, presentRect.w);
}

glm::vec2 RenderView::ScreenToWorld(const glm::vec2& panelPos) const
{
    // presentRect.y is a GL bottom-left offset; panelPos is top-down (Qt).
    // For centered letterboxing (the only layout this supports) the two
    // conventions coincide once converted -- computed properly rather than
    // assumed equal.
    const float topOffsetY = static_cast<float>(panelH - presentRect.y - presentRect.w);
    const glm::vec2 local(panelPos.x - static_cast<float>(presentRect.x), panelPos.y - topOffsetY);
    return camera->ScreenToWorld(local, presentRect.z, presentRect.w);
}

bool RenderView::ContainsPanelPixel(const glm::vec2& panelPos) const
{
    const float topOffsetY = static_cast<float>(panelH - presentRect.y - presentRect.w);
    return panelPos.x >= static_cast<float>(presentRect.x) &&
           panelPos.x <  static_cast<float>(presentRect.x + presentRect.z) &&
           panelPos.y >= topOffsetY &&
           panelPos.y <  topOffsetY + static_cast<float>(presentRect.w);
}

unsigned int RenderView::GetOutputTexture() const
{
    return pipeline->GetOutputTexture();
}
