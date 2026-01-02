#pragma once
#include "RenderCamera.hpp"
#include <glm/glm.hpp>

class GameCamera : public RenderCamera
{
public:
    static GameCamera& Get(){
        static GameCamera instance;
        return instance;
    }

    void Init() override;


void Update() override;

    void SetTarget(const glm::vec2& pos) { target = pos; }
    void SetFollowSpeed(float s) { followSpeed = s; }
    
private:
    GameCamera() = default;
    glm::vec2 target = {0.0f, 0.0f};
    float followSpeed = 5.0f;
};
