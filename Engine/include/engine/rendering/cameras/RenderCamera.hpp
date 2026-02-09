#pragma once

#include <glm/glm.hpp>

#include "engine/utils/EngineUtils.hpp"
class RenderCamera
{
public:
    // ───────────────────────────────────────────────────────
    // 1. Lifecycle
    // ───────────────────────────────────────────────────────
    virtual void Update();               // game/editor movement logic
    virtual void Resize(int w, int h);           // update projection bounds

    // ───────────────────────────────────────────────────────
    // 2. Transform API
    // ───────────────────────────────────────────────────────
    virtual void Init();
    void SetPosition(const glm::vec2& pos);
    void SetRotation(float degrees);
    void SetZoom(float zoomAmount);

    const glm::vec2& GetPosition() const;
    float GetRotation() const;
    float GetZoom() const;
    float GetOrthoSize() {return orthoSize;}

    // ───────────────────────────────────────────────────────
    // 3. Matrices
    // ───────────────────────────────────────────────────────
    const glm::mat4& GetViewMatrix();
    const glm::mat4& GetProjectionMatrix();
    
    // ───────────────────────────────────────────────────────
    // 4. Coordinate Conversion
    // ───────────────────────────────────────────────────────
    glm::vec2 ScreenToWorld(const glm::vec2& screenPos) const;
    glm::vec2 ScreenToWorld(const glm::vec2& screenPos, int widgetWidth, int widgetHeight) const;
    
    int viewportWidth  = 1;
    int viewportHeight = 1;

protected:
    // Recompute matrices only if dirty
    virtual void RecalculateView();
    virtual void RecalculateProjection();

protected:
    
    glm::vec2 position = {0.0f, 0.0f};
    float rotationDeg = 0.0f;      
    float zoom = 1.0f;  // 1.0 = normal zoom, higher = zoomed in              
    float orthoSize = 360.0f;  // Half-height in world units (pixels / PixelsPerUnit)


    glm::mat4 viewMatrix      = glm::mat4(1.0f);
    glm::mat4 projectionMatrix = glm::mat4(1.0f);

    bool viewDirty = true;
    bool projDirty = true;
};
