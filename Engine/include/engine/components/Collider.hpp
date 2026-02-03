#pragma once
#include "engine/components/Component.hpp"
#include "glm/glm.hpp"
#include "box2d/box2d.h"

class Collider : public Component{

public:

    virtual void Awake() override {};
    virtual void Update() override {};
    virtual void OnDestroy() override {};

    virtual void SetCenter(glm::vec2 center){};
    glm::vec2 GetCenter(){return center;}

    virtual void SetIsSensor(bool val){};
    virtual bool GetIsSensor(){return isSensor;}


    Collider() = default;

protected:
    bool isSensor = false;
    glm::vec2 center = {0 , 0};
     

};