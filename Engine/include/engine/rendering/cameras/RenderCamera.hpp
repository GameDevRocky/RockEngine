#pragma once

#include <glm/glm.hpp>

#include "engine/utils/EngineUtils.hpp"
class RenderCamera
{
public:

    virtual void Update();              
    virtual void Resize(int w, int h);       

    virtual void Init();
    void SetPosition(const glm::vec2& pos);
    void SetRotation(float degrees);
    void SetZoom(float zoomAmount);

    const glm::vec2& GetPosition() const;
    float GetRotation() const;
    float GetZoom() const;
    float GetOrthoSize() {return orthoSize;}

    const glm::mat4& GetViewMatrix();
    const glm::mat4& GetProjectionMatrix();
   
    glm::vec2 ScreenToWorld(const glm::vec2& screenPos) const;
    glm::vec2 ScreenToWorld(const glm::vec2& screenPos, int widgetWidth, int widgetHeight) const;
    
    int viewportWidth  = 1;
    int viewportHeight = 1;

protected:
    virtual void RecalculateView();
    virtual void RecalculateProjection();

protected:
    
    glm::vec2 position = {0.0f, 0.0f};
    float rotationDeg = 0.0f;      
    float zoom = 1.0f;  
    float orthoSize = 360.0f; 


    glm::mat4 viewMatrix      = glm::mat4(1.0f);
    glm::mat4 projectionMatrix = glm::mat4(1.0f);

    bool viewDirty = true;
    bool projDirty = true;
};
