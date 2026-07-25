#include "engine/rendering/cameras/EditorCamera.hpp"
#include <glm/glm.hpp>

void EditorCamera::Resize(int w, int h)
{
    RenderCamera::Resize(w, h);
    // 1 world unit == 1 pixel at zoom 1: halfHeight = orthoSize/zoom must equal
    // pixelHeight/2, so orthoSize = pixelHeight/2.
    SetOrthoSize(static_cast<float>(h) / 2.0f);
}

void EditorCamera::ZoomAt(const glm::vec2& screenPos, float scrollDelta)
{
    const int vw = GetPixelWidth();
    const int vh = GetPixelHeight();
    const glm::vec2 before = ScreenToWorld(screenPos, vw, vh);

    const float factor = 1.0f + scrollDelta / 1200.0f;
    SetZoom(glm::clamp(GetZoom() * factor, minZoom, maxZoom));

    const glm::vec2 after = ScreenToWorld(screenPos, vw, vh);
    SetPosition(GetPosition() + (before - after));
}

void EditorCamera::PanByPixels(const glm::vec2& pixelDelta)
{
    const float worldHeight = (GetOrthoSize() / GetZoom()) * 2.0f;
    const float unitsPerPixel = worldHeight / static_cast<float>(GetPixelHeight());

    const glm::vec2 worldDelta(
        -pixelDelta.x * unitsPerPixel,
         pixelDelta.y * unitsPerPixel
    );

    SetPosition(GetPosition() + worldDelta);
}
