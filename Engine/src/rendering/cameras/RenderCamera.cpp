#include "engine/rendering/cameras/RenderCamera.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

void RenderCamera::Update()
{

}

void RenderCamera::Resize(int w, int h)
{
    viewportWidth  = w;
    viewportHeight = h;
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
    zoom = zoomAmount;
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
    glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(-position, 0.0f));
    transform = glm::rotate(transform, glm::radians(-rotationDeg), glm::vec3(0, 0, 1));
    
    viewMatrix = transform;   
    viewDirty = false;
}

glm::vec2 RenderCamera::ScreenToWorld(const glm::vec2& screenPos) const
{
  
    float ndcX = (2.0f * screenPos.x) / viewportWidth - 1.0f;
    float ndcY = 1.0f - (2.0f * screenPos.y) / viewportHeight; 
    
    glm::vec4 ndcPos = glm::vec4(ndcX, ndcY, 0.0f, 1.0f);
    
    glm::mat4 invProj = glm::inverse(const_cast<RenderCamera*>(this)->GetProjectionMatrix());
    glm::mat4 invView = glm::inverse(const_cast<RenderCamera*>(this)->GetViewMatrix());
    
    glm::vec4 viewPos = invProj * ndcPos;
    glm::vec4 worldPos = invView * viewPos;
    
    return glm::vec2(worldPos.x, worldPos.y);
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
    float aspect = (float)viewportWidth / (float)viewportHeight;

    float halfHeight = orthoSize / zoom;
    float halfWidth  = halfHeight * aspect;  

    projectionMatrix = glm::ortho(
        -halfWidth, halfWidth,
        -halfHeight, halfHeight,
        -1.0f, 1.0f
    );

    projDirty = false;
}


void RenderCamera::Init(){
    
}