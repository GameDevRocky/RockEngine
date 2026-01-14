#pragma once
#include "RenderCamera.hpp"
#include <glm/glm.hpp>

class SceneCamera : public RenderCamera
{
public:
    // Editor input (WASD / mouse drag)
    void Update() override;
    void Init() override;

    // Editor sets movement speed
    void SetMoveSpeed(float speed) { moveSpeed = speed; }
    void SetZoomSpeed(float speed) { zoomSpeed = speed; }

private:
    SceneCamera() = default;
    float moveSpeed = 5.0f;
    float zoomSpeed = 1.0f;
};
