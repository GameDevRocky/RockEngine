#include "engine/rendering/cameras/RenderCamera.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

void RenderCamera::Update()
{

}

void RenderCamera::Resize(int w, int h)
{
    pixelWidth  = w;
    pixelHeight = h;
    projDirty = true;
}

void RenderCamera::SetPosition(const glm::vec2& pos)
{
    position = pos;
    viewDirty = true;
}

void RenderCamera::SetRotation(float degrees)
{
    rotationDeg = degrees;
    viewDirty = true;

}

void RenderCamera::SetZoom(float zoomAmount)
{
    // Hard floor: zoom feeds a division (halfHeight = orthoSize / zoom), so a
    // zero/negative value would blow up the projection.
    zoom = glm::max(zoomAmount, kMinZoom);
    projDirty = true;
}

void RenderCamera::SetOrthoSize(float size)
{
    orthoSize = glm::max(size, kMinOrthoSize);
    projDirty = true;
}

const glm::vec2& RenderCamera::GetPosition() const { return position; }
float RenderCamera::GetRotation() const { return rotationDeg; }
float RenderCamera::GetZoom() const { return zoom; }

const glm::mat4& RenderCamera::GetViewMatrix()
{
    if (viewDirty)
        RecalculateView();
    return viewMatrix;
}

const glm::mat4& RenderCamera::GetProjectionMatrix()
{
    if (projDirty)
        RecalculateProjection();
    return projectionMatrix;
}

void RenderCamera::RecalculateView()
{
    // View = R(-theta) * T(-pos). glm::rotate/translate post-multiply, so
    // translate(rotate(I, -theta), -pos) composes as R(-theta) * T(-pos).
    // (The previous order T*R orbited the world origin instead of rolling in place.)
    glm::mat4 transform = glm::rotate(glm::mat4(1.0f), glm::radians(-rotationDeg), glm::vec3(0, 0, 1));
    transform = glm::translate(transform, glm::vec3(-position, 0.0f));

    viewMatrix = transform;
    viewDirty = false;
}

glm::vec2 RenderCamera::ScreenToWorld(const glm::vec2& screenPos, int widgetWidth, int widgetHeight) const
{

    float ndcX = (2.0f * screenPos.x) / widgetWidth - 1.0f;
    float ndcY = 1.0f - (2.0f * screenPos.y) / widgetHeight;

    glm::vec4 ndcPos = glm::vec4(ndcX, ndcY, 0.0f, 1.0f);

    glm::mat4 invProj = glm::inverse(const_cast<RenderCamera*>(this)->GetProjectionMatrix());
    glm::mat4 invView = glm::inverse(const_cast<RenderCamera*>(this)->GetViewMatrix());

    glm::vec4 viewPos = invProj * ndcPos;
    glm::vec4 worldPos = invView * viewPos;

    return glm::vec2(worldPos.x, worldPos.y);
}

void RenderCamera::RecalculateProjection()
{
    // The pixel extent this camera actually draws into -- viewportRect narrows
    // it for split-screen. Default {0,0,1,1} makes this identical to using the
    // full pixel dims.
    const float pw = glm::max(1.0f, viewportRect.z * static_cast<float>(pixelWidth));
    const float ph = glm::max(1.0f, viewportRect.w * static_cast<float>(pixelHeight));
    const float aspect = pw / ph;

    const float halfHeight = orthoSize / zoom;
    const float halfWidth  = halfHeight * aspect;

    projectionMatrix = glm::ortho(
        -halfWidth, halfWidth,
        -halfHeight, halfHeight,
        nearClip, farClip
    );

    projDirty = false;
}


void RenderCamera::Init(){

}
