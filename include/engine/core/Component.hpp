#pragma once
#include "engine/core/GameObject.hpp"

class Component {
public:
virtual ~Component() = default;
virtual void Awake();
virtual void Start();
virtual void Update();
virtual void LateUpdate();
virtual void FixedUpdate();

private:
int id = -1;
GameObject* gameObject = nullptr;
bool enabled = true;



};