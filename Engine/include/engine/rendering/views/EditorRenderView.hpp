#pragma once

#include <string>
#include <vector>

#include "engine/rendering/views/RenderView.hpp"
#include "engine/rendering/cameras/EditorCamera.hpp"

class PickingPass;

// The Scene view's render view: Clear+Grid (setup), Scene+Debug (per scene),
// Picking (finalize). Its camera is always an EditorCamera with a free
// (fills-the-panel) target aspect -- pixel-identical to the pre-refactor
// SceneViewGui behavior.
class EditorRenderView : public RenderView
{
public:
    void Init() override;

    // fbPixelX/Y: DPI-scaled pixel coords in THIS view's render-target space
    // (GL bottom-left origin, y measured from the bottom) -- i.e. already
    // un-letterboxed. Since this view's aspect is always free, its target
    // always fills the panel 1:1, so callers can pass framebuffer coords
    // directly without going through ScreenToWorld's un-mapping.
    // Returns the picked GameObject id, or "" for nothing.
    std::string Pick(int fbPixelX, int fbPixelY);

    // Rect version of Pick(), for drag box-select: every GameObject id anywhere
    // under the rect (width/height in the same framebuffer-pixel units as Pick).
    std::vector<std::string> PickRect(int fbPixelX, int fbPixelY, int width, int height);

    EditorCamera* GetEditorCamera() const { return static_cast<EditorCamera*>(camera); }

private:
    PickingPass* pickingPass = nullptr;   // borrowed; the pipeline owns and deletes it
};
