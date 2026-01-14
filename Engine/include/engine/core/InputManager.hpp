#pragma once
#include "engine/core/System.hpp"
#include <glm/glm.hpp>

class InputManager : public System {
public:

    void Init() override;
    void PostInit() override {};
    void Update() override;
    void Shutdown() override;

    glm::vec2 GetMousePosition(){return mouse_pos;}
    void SetMousePosition(glm::vec2 pos){ mouse_pos = pos;}

    void SetKeyState(int key, bool pressed) { m_keyStates[key] = pressed; }
    bool IsKeyDown(int key) { return m_keyStates[key]; }
    
    InputManager() = default;
    ~InputManager() override = default;


private:
    std::unordered_map<int, bool> m_keyStates;
    glm::vec2 mouse_pos;
};
