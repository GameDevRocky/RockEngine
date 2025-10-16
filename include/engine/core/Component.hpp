#pragma once

class GameObject; // forward declaration to avoid circular include

class Component {
public:
    GameObject* gameObject = nullptr;
    virtual ~Component() = default;
    virtual void Update();
};