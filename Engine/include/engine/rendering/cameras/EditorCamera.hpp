#pragma once

#include "engine/rendering/cameras/RenderCamera.hpp"

// The editor viewport's navigation camera. Deliberately OUTSIDE the ECS: never
// serialized, never deep-copied on play mode, never in the hierarchy -- owned
// directly by EditorRenderView. Exists to hold the pan/zoom math that
// previously lived in SceneViewGui's Qt event handlers.
class EditorCamera : public RenderCamera
{
public:
    // Zoom-to-cursor. screenPos: FRAMEBUFFER-scale (DPI-scaled) pixel
    // position -- i.e. mousePos * devicePixelRatioF() -- matching this
    // camera's own GetPixelWidth()/GetPixelHeight() (set via Resize(fbw,fbh)).
    // Keeps orthoSize = pixelHeight / 2 so the editor view is 1 world unit = 1
    // screen pixel at zoom 1 (pixel-perfect), and stays that way across window
    // resizes -- content holds its pixel size rather than rescaling. Scroll zoom
    // multiplies on top via the base's `zoom`.
    void Resize(int w, int h) override;

    void ZoomAt(const glm::vec2& screenPos, float scrollDelta);

    // pixelDelta: mouse delta in LOGICAL pixels (Qt's QMouseEvent::pos() is
    // not DPI-scaled). NOTE: this mixes a logical delta with this camera's
    // DPI-scaled GetPixelHeight() internally -- ported verbatim from the
    // pre-refactor SceneViewGui::mouseMoveEvent, which has the same mismatch
    // (pan speed is under-scaled by devicePixelRatio() on non-1.0 DPI
    // displays). Not fixed here; out of this refactor's bug-fix scope.
    void PanByPixels(const glm::vec2& pixelDelta);

    void SetZoomLimits(float minZ, float maxZ) { minZoom = minZ; maxZoom = maxZ; }

private:
    float minZoom = 0.05f;
    float maxZoom = 100.0f;
};
