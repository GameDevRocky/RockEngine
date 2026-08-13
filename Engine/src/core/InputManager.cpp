#include "engine/core/InputManager.hpp"
#include "engine/input/GamepadService.hpp"
#include <algorithm>
#include <iostream>

void InputManager::Init() {
    mouse_pos = {0, 0};
    std::cout << "InputManager Initialized" << std::endl;
}

void InputManager::Update(){
    m_keysJustPressed = m_keysJustPressedQueue;
    m_keysJustPressedQueue.clear();

    m_mouseButtonsJustPressed = m_mouseButtonsJustPressedQueue;
    m_mouseButtonsJustPressedQueue.clear();

    // Gamepad edge latch. GamepadService::Update() has already run this frame (Engine::Update
    // polls it before ticking the active Container), so this reads fresh hardware state, not
    // last frame's.
    const GamepadService& pads = GamepadService::Get();
    for (int p = 0; p < kMaxGamepads; ++p) {
        const GamepadState& s = pads.GetState(p);
        for (int b = 0; b < kPadButtonCount; ++b) {
            m_padButtonsPrev[p][b] = m_padButtons[p][b];
            m_padButtons[p][b]     = s.buttons[b];
        }
    }
}

void InputManager::Shutdown() {
    std::cout << "InputManager shutting down" << std::endl;
}

// --- Gamepad ---------------------------------------------------------------------------------

int InputManager::GetGamepadCount() const {
    return GamepadService::Get().GetPadCount();
}

bool InputManager::IsGamepadConnected(int pad) const {
    return GamepadService::Get().IsConnected(pad);
}

bool InputManager::IsGamepadButtonDown(int pad, GamepadButton button) const {
    const int b = static_cast<int>(button);
    if (pad < 0 || pad >= kMaxGamepads || b < 0 || b >= kPadButtonCount)
        return false;
    return m_padButtons[pad][b];
}

bool InputManager::IsGamepadButtonPressed(int pad, GamepadButton button) const {
    const int b = static_cast<int>(button);
    if (pad < 0 || pad >= kMaxGamepads || b < 0 || b >= kPadButtonCount)
        return false;
    return m_padButtons[pad][b] && !m_padButtonsPrev[pad][b];
}

bool InputManager::IsGamepadButtonReleased(int pad, GamepadButton button) const {
    const int b = static_cast<int>(button);
    if (pad < 0 || pad >= kMaxGamepads || b < 0 || b >= kPadButtonCount)
        return false;
    return !m_padButtons[pad][b] && m_padButtonsPrev[pad][b];
}

float InputManager::GetGamepadAxis(int pad, GamepadAxis axis) const {
    const int a = static_cast<int>(axis);
    if (a < 0 || a >= static_cast<int>(GamepadAxis::Count))
        return 0.0f;
    return GamepadService::Get().GetState(pad).axes[a];
}

glm::vec2 InputManager::GetGamepadStick(int pad, GamepadStick stick) const {
    const GamepadState& s = GamepadService::Get().GetState(pad);

    const glm::vec2 raw = (stick == GamepadStick::Left)
        ? glm::vec2{ s.axes[static_cast<int>(GamepadAxis::LeftX)],
                     s.axes[static_cast<int>(GamepadAxis::LeftY)] }
        : glm::vec2{ s.axes[static_cast<int>(GamepadAxis::RightX)],
                     s.axes[static_cast<int>(GamepadAxis::RightY)] };

    const float len = glm::length(raw);
    const float dz  = std::clamp(m_stickDeadzone, 0.0f, 0.95f);
    if (len <= dz || len <= 0.0f)
        return { 0.0f, 0.0f };

    // Rescale what is left of the range back to 0..1 so movement ramps from a standstill
    // instead of snapping to the deadzone magnitude the instant the stick clears it.
    const float scaled = std::min((len - dz) / (1.0f - dz), 1.0f);
    return raw * (scaled / len);
}

const GamepadState& InputManager::GetGamepadState(int pad) const {
    return GamepadService::Get().GetState(pad);
}

void InputManager::SetGamepadRumble(int pad, float low, float high, float durationSeconds) {
    GamepadService::Get().SetRumble(pad, low, high, durationSeconds);
}

void InputManager::StopGamepadRumble(int pad) {
    GamepadService::Get().StopRumble(pad);
}

void InputManager::SetGamepadLightColor(int pad, int r, int g, int b) {
    const auto clamp8 = [](int v) {
        return static_cast<std::uint8_t>(std::clamp(v, 0, 255));
    };
    GamepadService::Get().SetLightColor(pad, clamp8(r), clamp8(g), clamp8(b));
}

void InputManager::SeedGamepadButtons() {
    const GamepadService& pads = GamepadService::Get();
    for (int p = 0; p < kMaxGamepads; ++p) {
        const GamepadState& s = pads.GetState(p);
        for (int b = 0; b < kPadButtonCount; ++b) {
            m_padButtons[p][b]     = s.buttons[b];
            m_padButtonsPrev[p][b] = s.buttons[b];
        }
    }
}

InputManager* InputManager::Copy(){
    InputManager* input = new InputManager();
    input->mouse_pos = mouse_pos;
    input->m_keyStates = m_keyStates;
    input->m_stickDeadzone = m_stickDeadzone;

    // Seeded from live hardware rather than copied from this manager: both buffers start equal,
    // so a button already held when play mode starts reads as down but never fires a press edge
    // on the runtime container's first frame.
    input->SeedGamepadButtons();
    return input;
}

InputManager* InputManager::Copy(Container* container){
    InputManager* copy = this->Copy();
    copy->Attach(container);
    return copy;
}
