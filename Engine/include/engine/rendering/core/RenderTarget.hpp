#pragma once

// RGBA8 color texture + DEPTH24_STENCIL8 renderbuffer. An internal detail of
// RenderPipeline -- deliberately NOT generalized to serve PickingPass, which
// needs an R32UI color attachment and a depth *texture* (for glReadPixels)
// rather than a renderbuffer. Revisit if a third consumer needs the same shape.
//
// Every method requires a current GL context.
class RenderTarget
{
public:
    RenderTarget() = default;
    ~RenderTarget();

    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;

    // No-op if width/height are unchanged from the last successful resize.
    // Returns false (and leaves the target in its previous state) if the FBO
    // comes back incomplete.
    bool Resize(int width, int height);
    void Destroy();   // idempotent; safe to call even if never created

    unsigned int GetFBO() const          { return fbo; }
    unsigned int GetColorTexture() const { return colorTexture; }
    int  GetWidth() const  { return width; }
    int  GetHeight() const { return height; }
    bool IsValid() const   { return fbo != 0; }

private:
    unsigned int fbo = 0;
    unsigned int colorTexture = 0;
    unsigned int depthRBO = 0;
    int width = 0;
    int height = 0;
};
