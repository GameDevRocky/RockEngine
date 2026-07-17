#pragma once

#include "engine/rendering/views/RenderView.hpp"

// The Game view's render view: Clear (setup), Scene (per scene). Its camera
// is the base RenderCamera -- until the Camera component lands, it stays at
// a free target aspect and the origin/zoom/orthoSize defaults, matching the
// pre-refactor GameViewGui behavior exactly.
class GameRenderView : public RenderView
{
public:
    void Init() override;

    // True if the last UpdateCamera() resolved a live main Camera (i.e.
    // Camera::GetMain() found an enabled Camera on an active GameObject).
    // Hosts use this to overlay a "no cameras rendering" notice. Reflects the
    // most recently rendered frame, so read it after Render().
    bool HasActiveCamera() const { return hasActiveCamera; }

protected:
    void UpdateCamera() override;

private:
    bool hasActiveCamera = false;
};
