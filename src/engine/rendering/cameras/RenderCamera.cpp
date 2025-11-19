#include "engine/rendering/cameras/RenderCamera.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// ───────────────────────────────────────────────────────
// 1. Lifecycle
// ───────────────────────────────────────────────────────
void RenderCamera::Update()
{
    // Base camera does nothing. Derived classes can implement movement.
}

void RenderCamera::Resize(int w, int h)
{
    viewportWidth  = w;
    viewportHeight = h;
    projDirty = true;
}

// ───────────────────────────────────────────────────────
// 2. Transform API
// ───────────────────────────────────────────────────────
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
    zoom = zoomAmount;
    projDirty = true;
}

const glm::vec2& RenderCamera::GetPosition() const { return position; }
float RenderCamera::GetRotation() const { return rotationDeg; }
float RenderCamera::GetZoom() const { return zoom; }

// ───────────────────────────────────────────────────────
// 3. Matrices
// ───────────────────────────────────────────────────────
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

// ───────────────────────────────────────────────────────
// 4. Internal matrix calculation
// ───────────────────────────────────────────────────────
void RenderCamera::RecalculateView()
{
    glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(-position, 0.0f));
    transform = glm::rotate(transform, glm::radians(-rotationDeg), glm::vec3(0, 0, 1));
    
    viewMatrix = transform;   // ← no zoom here
    viewDirty = false;
}

void RenderCamera::RecalculateProjection()
{
    float aspect = (float)viewportWidth / (float)viewportHeight;

    float halfHeight = orthoSize / zoom;

    float halfWidth  = halfHeight * aspect;    // maintain aspect ratio

    projectionMatrix = glm::ortho(
        -halfWidth, halfWidth,
        -halfHeight, halfHeight,
        -1.0f, 1.0f
    );

    projDirty = false;
}


void RenderCamera::Init(){
    
}