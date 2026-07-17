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

protected:
    void UpdateCamera() override;
};
