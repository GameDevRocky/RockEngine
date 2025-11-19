#pragma once

#include <glm/glm.hpp>


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
    int viewportWidth  = 1;
    int viewportHeight = 1;

protected:
    // Recompute matrices only if dirty
    virtual void RecalculateView();
    virtual void RecalculateProjection();

protected:
    
    glm::vec2 position = {0.0f, 0.0f};
    float rotationDeg = 0.0f;      
    float zoom = 1.0f;              
    float orthoSize = 5.0f;


    glm::mat4 viewMatrix      = glm::mat4(1.0f);
    glm::mat4 projectionMatrix = glm::mat4(1.0f);

    bool viewDirty = true;
    bool projDirty = true;
};
