#pragma once

#include <glm/glm.hpp>
#include <string>
#include <unordered_set>

#include "engine/utils/EngineUtils.hpp"

// The RESOLVED per-view render state: everything a RenderPass needs to know
// about one view for one frame -- pose, matrices, clear settings, culling
// mask, viewport rect. Owned by a viewport (a Qt widget today; a RenderView
// once the rendering lifecycle moves into Engine), never by a Container, never
// serialized.
//
// The forthcoming AUTHORED counterpart is a Camera component, which will push
// its settings into a RenderCamera once per frame rather than this type living
// inside the ECS itself -- viewport pixel dims are a property of the view, not
// the camera, so two views rendering one camera at different sizes must not
// fight over one set of dims.
class RenderCamera
{
public:
    enum class ClearFlags { SolidColor, DepthOnly, Nothing };

    // Perspective is intentionally not offered: Transform::localPosition is a
    // glm::vec2, so every object sits at z=0 and a perspective camera would
    // render a degenerate flat plane at the eye plane. Revisit once Transform
    // gains a Z.
    enum class Projection { Orthographic };

    virtual void Update();
    virtual void Resize(int w, int h);

    virtual void Init();
    void SetPosition(const glm::vec2& pos);
    void SetRotation(float degrees);
    void SetZoom(float zoomAmount);
    void SetOrthoSize(float size);
    void SetProjection(Projection p) { projection = p; projDirty = true; }
    void SetClipPlanes(float nearZ, float farZ) { nearClip = nearZ; farClip = farZ; projDirty = true; }

    const glm::vec2& GetPosition() const;
    float GetRotation() const;
    float GetZoom() const;
    float GetOrthoSize() const {return orthoSize;}
    Projection GetProjection() const { return projection; }
    float GetNearClip() const { return nearClip; }
    float GetFarClip() const { return farClip; }

    // ---- clear ----
    void SetClearFlags(ClearFlags f) { clearFlags = f; }
    void SetClearColor(const glm::vec4& c) { clearColor = c; }
    ClearFlags GetClearFlags() const { return clearFlags; }
    const glm::vec4& GetClearColor() const { return clearColor; }

    // ---- culling (sorting-layer NAME set test; NOT spatial/frustum culling) ----
    // Empty set == no mask == draw everything. Deliberately a name set, not a
    // bitmask: LayerManager re-sorts layers by priority, so a layer's index is
    // not stable across an edit -- a bitmask keyed on index would silently
    // remap which layers a camera sees.
    void SetCullingLayers(std::unordered_set<std::string> layers) { cullingLayers = std::move(layers); }
    const std::unordered_set<std::string>& GetCullingLayers() const { return cullingLayers; }
    bool PassesCullingMask(const std::string& sortingLayer) const {
        return cullingLayers.empty() || cullingLayers.count(sortingLayer) > 0;
    }

    // ---- viewport rect: normalized {x, y, w, h} sub-region of the render
    // target this camera draws into, e.g. for split-screen. Default {0,0,1,1}
    // covers the whole target. ----
    void SetViewportRect(const glm::vec4& r) { viewportRect = r; projDirty = true; }
    const glm::vec4& GetViewportRect() const { return viewportRect; }

    // ---- target aspect: the aspect ratio the owning RenderView should size
    // its render target to (letterboxing the remainder of the panel). <= 0
    // means "free" -- the view fills the panel exactly, which is today's
    // behavior and the default here. A Camera component may author a fixed
    // aspect (e.g. 16:9) via ApplyTo(). ----
    void SetTargetAspect(float a) { targetAspect = a; }
    float GetTargetAspect() const { return targetAspect; }

    const glm::mat4& GetViewMatrix();
    const glm::mat4& GetProjectionMatrix();

    // screenPos: pixels, origin top-left, relative to a viewWidth x viewHeight view.
    // NOTE: there is deliberately no 1-arg overload using the camera's own
    // pixel dims -- those are DPI-scaled framebuffer dims while on-screen mouse
    // events report logical widget coords, so a 1-arg version silently
    // mismatches on any display with devicePixelRatio() != 1.
    glm::vec2 ScreenToWorld(const glm::vec2& screenPos, int widgetWidth, int widgetHeight) const;

    int GetPixelWidth() const  { return pixelWidth; }
    int GetPixelHeight() const { return pixelHeight; }

    static constexpr float kMinZoom      = 1e-4f;
    static constexpr float kMinOrthoSize = 1e-4f;

protected:
    virtual void RecalculateView();
    virtual void RecalculateProjection();

    glm::vec2 position = {0.0f, 0.0f};
    float rotationDeg = 0.0f;
    float zoom = 1.0f;
    float orthoSize = 360.0f;

    Projection projection = Projection::Orthographic;
    float nearClip = -1.0f;   // matches the projection's previous hardcoded (-1, 1) clip range
    float farClip  =  1.0f;

    ClearFlags clearFlags = ClearFlags::SolidColor;
    glm::vec4  clearColor = { 30.0f/255.0f, 30.0f/255.0f, 30.0f/255.0f, 1.0f };  // matches ClearPass's previous hardcoded color

    std::unordered_set<std::string> cullingLayers;
    glm::vec4 viewportRect = { 0.0f, 0.0f, 1.0f, 1.0f };
    float targetAspect = 0.0f;   // free/fills-the-panel by default

    int pixelWidth  = 1;
    int pixelHeight = 1;

    glm::mat4 viewMatrix      = glm::mat4(1.0f);
    glm::mat4 projectionMatrix = glm::mat4(1.0f);

    bool viewDirty = true;
    bool projDirty = true;
};
