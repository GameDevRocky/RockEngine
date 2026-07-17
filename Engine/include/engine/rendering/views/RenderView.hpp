#pragma once

#include <glm/glm.hpp>

#include "engine/rendering/pipelines/RenderPipeline.hpp"
#include "engine/rendering/cameras/RenderCamera.hpp"
#include "Engine.hpp"   // EngineUtils::Proxy, SceneManager

// One output image: a RenderPipeline + a resolved RenderCamera + the target
// sizing/letterbox policy for a single viewport (editor or game). Owned by
// Renderer, never by a Container -- render resources have no per-world
// identity, so a view must survive the play-mode container swap untouched.
//
// Resolves its own scenes each frame via a Proxy<SceneManager>, so the host
// (a Qt widget) never touches Engine/Scene types -- it only calls Render()
// then Present() with an FBO id.
class RenderView
{
public:
    virtual ~RenderView();

    virtual void Init() = 0;   // subclass composes its pass list + camera type

    // panelPixelW/H: DPI-scaled framebuffer size of the HOST PANEL (not the
    // render target -- those may differ once a target aspect is set).
    void Resize(int panelPixelW, int panelPixelH);

    // Pulls authored camera settings (UpdateCamera), re-fits the render
    // target to the camera's target aspect if it changed, then runs the
    // pipeline.
    void Render();

    // Blits the output texture into targetFBO, letterboxed to fit
    // panelPixelW/H if the camera has a fixed target aspect.
    void Present(unsigned int targetFBO, int panelPixelW, int panelPixelH);

    // panelPos: pixels, DPI-SCALED (framebuffer-space), origin top-left of
    // the panel -- i.e. a Qt mouse event's pos() multiplied by
    // devicePixelRatioF(), matching panelW/panelH (set via Resize(), which
    // hosts must call with DPI-scaled dims). Un-maps the letterbox
    // offset/scale before delegating to the camera. Hosts must go through
    // this, never camera->ScreenToWorld directly, once letterboxing is live.
    glm::vec2 ScreenToWorld(const glm::vec2& panelPos) const;
    bool ContainsPanelPixel(const glm::vec2& panelPos) const;

    RenderCamera* GetCamera() const { return camera; }
    unsigned int GetOutputTexture() const;

    // ---- Editor display-sizing override (the Game view's Unity-style aspect /
    // resolution selector). Overrides how the render target is sized/fitted for
    // DISPLAY only -- it never touches authored Camera settings, so it survives
    // the play-mode container swap with the view. Default `FromCamera`
    // reproduces the pre-existing behavior and is what the Scene view uses. ----
    enum class DisplayMode { FromCamera, Free, FixedAspect, FixedResolution };

    // Defer to the resolved camera's target aspect (the default).
    void SetDisplayFromCamera() { displayMode = DisplayMode::FromCamera; }
    // Fill the panel exactly, ignoring any camera target aspect.
    void SetDisplayFree()       { displayMode = DisplayMode::Free; }
    // Letterbox to a fixed aspect ratio (w/h), fitting the panel.
    void SetDisplayAspect(float aspect);
    // Render at an exact internal resolution, scaled (letterboxed) to fit the panel.
    void SetDisplayResolution(int w, int h);

    DisplayMode GetDisplayMode() const { return displayMode; }
    float       GetDisplayAspect() const { return displayAspect; }
    glm::ivec2  GetDisplayResolution() const { return { fixedResW, fixedResH }; }

protected:
    RenderView() = default;
    virtual void UpdateCamera() {}   // pull authored settings into `camera`

    void ApplyTargetSizing();   // fits targetAspect; no-op if target dims unchanged

    RenderPipeline* pipeline = nullptr;   // owned
    RenderCamera*   camera   = nullptr;   // owned

    int panelW = 1, panelH = 1;     // DPI-scaled panel dims (from Resize)
    int targetW = 0, targetH = 0;   // DPI-scaled render-target dims (from ApplyTargetSizing)

    // Editor display-sizing override (see the DisplayMode setters above).
    DisplayMode displayMode = DisplayMode::FromCamera;
    float       displayAspect = 16.0f / 9.0f;   // used by FixedAspect
    int         fixedResW = 1920, fixedResH = 1080;   // used by FixedResolution

    // Where the target texture lands within the panel when Present() blits
    // it: {x, y, w, h} in DPI-scaled pixels, GL bottom-left origin.
    glm::ivec4 presentRect = {0, 0, 1, 1};

    EngineUtils::Proxy<SceneManager> sceneManager;
};
