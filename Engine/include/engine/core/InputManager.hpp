#pragma once
#include "engine/core/System.hpp"
#include <glm/glm.hpp>
#include <unordered_set>

class InputManager : public System {
public:

    void Init() override;
    void Update() override;
    void Shutdown() override;

    glm::vec2 GetMousePosition(){return mouse_pos;}
    void SetMousePosition(glm::vec2 pos){ mouse_pos = pos;}

    void SetKeyState(int key, bool pressed) {
        if (pressed && !m_keyStates[key])
            m_keysJustPressedQueue.insert(key);
        m_keyStates[key] = pressed;
    }
    bool IsKeyDown(int key) { return m_keyStates[key]; }
    bool IsKeyPressed(int key) { return m_keysJustPressed.count(key) > 0; }

    void SetMouseButtonState(int button, bool pressed) {
        if (pressed && !m_mouseButtonStates[button])
            m_mouseButtonsJustPressedQueue.insert(button);
        m_mouseButtonStates[button] = pressed;
    }
    bool IsMouseButtonDown(int button) { return m_mouseButtonStates[button]; }
    bool IsMouseButtonPressed(int button) { return m_mouseButtonsJustPressed.count(button) > 0; }

    InputManager() = default;
    ~InputManager() override = default;

    InputManager* Copy() override;
    InputManager* Copy(Container* container) override;


private:
    std::unordered_map<int, bool> m_keyStates;
    std::unordered_set<int> m_keysJustPressedQueue;
    std::unordered_set<int> m_keysJustPressed;

    std::unordered_map<int, bool> m_mouseButtonStates;
    std::unordered_set<int> m_mouseButtonsJustPressedQueue;
    std::unordered_set<int> m_mouseButtonsJustPressed;

    glm::vec2 mouse_pos;
};
