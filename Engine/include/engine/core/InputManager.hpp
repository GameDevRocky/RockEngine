#pragma once
#include "engine/core/System.hpp"
#include "engine/input/GamepadTypes.hpp"
#include <glm/glm.hpp>
#include <unordered_set>

// Which stick GetGamepadStick reads. Not part of GamepadAxis because a stick is a vec2 pair,
// not an axis, and callers overwhelmingly want the pair (with deadzone applied as one radial
// test) rather than two independently-deadzoned scalars.
enum class GamepadStick : int { Left = 0, Right };

// Owned by Renderer and outlives any Container; only its ScreenToWorld is needed here, so a
// forward declaration keeps the rendering headers out of every scripting translation unit.
class RenderView;

class InputManager : public System {
public:

    void Init() override;
    void Update() override;
    void Shutdown() override;

    // --- Mouse position -------------------------------------------------------------------
    // The stored position is SCREEN space (DPI-scaled panel pixels, top-left origin) plus the
    // view that space belongs to -- never a world coordinate. World space is a function of the
    // screen position AND the camera, so caching the world result freezes it against whatever
    // the camera happened to be on the last motion event: pan or zoom without moving the mouse
    // and every script reading get_mouse_pos() aims at a stale point. Resolving on read makes
    // the value correct on any frame regardless of when the mouse last moved, and costs one
    // matrix un-project per query rather than per frame.
    //
    // `source` is the RenderView the panelPos came from. Views are owned by Renderer, not by a
    // Container, so the pointer stays valid across the play-mode container swap. Last writer
    // wins when both the Scene and Game views see motion -- which is the behaviour that was
    // already in place when both pushed world positions into the same field.
    void SetMouseScreenPosition(RenderView* source, glm::vec2 panelPos) {
        m_mouseSourceView = source;
        m_mouseScreenPos  = panelPos;
    }

    glm::vec2 GetMouseScreenPosition() const { return m_mouseScreenPos; }

    // World-space cursor, resolved against the source view's CURRENT camera. Falls back to the
    // raw screen position before any host has pushed a view.
    glm::vec2 GetMousePosition() const;

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

    // --- Gamepad --------------------------------------------------------------------------
    // The devices themselves are global (GamepadService, outside any Container -- hardware has
    // no per-world identity). What lives HERE is the per-container edge state: which buttons
    // changed between this frame and last. That split means the editor and runtime containers
    // each get their own clean pressed/released stream off one shared physical pad, exactly
    // like AudioSource components sharing one AudioEngine device.
    //
    // Unlike the keyboard path above (which is pushed in from Qt events and so only tracks a
    // just-pressed queue), gamepads are polled, so a real previous/current double buffer is
    // available and released-edges come for free.

    int  GetGamepadCount() const;
    bool IsGamepadConnected(int pad = 0) const;

    bool IsGamepadButtonDown(int pad, GamepadButton button) const;
    bool IsGamepadButtonPressed(int pad, GamepadButton button) const;    // first frame only
    bool IsGamepadButtonReleased(int pad, GamepadButton button) const;   // first frame only

    // Raw, deadzone-free, [-1,1] (triggers [0,1]) with stick Y already pointing up.
    float GetGamepadAxis(int pad, GamepadAxis axis) const;

    // Radial deadzone applied and the remainder rescaled to a full 0..1 magnitude, so a stick
    // just past the threshold yields a near-zero vector instead of jumping to the deadzone
    // value. Per-axis deadzoning is deliberately avoided -- it makes diagonals reach further
    // than cardinals and produces the classic "square" feel.
    glm::vec2 GetGamepadStick(int pad, GamepadStick stick) const;

    float GetStickDeadzone() const { return m_stickDeadzone; }
    void  SetStickDeadzone(float dz) { m_stickDeadzone = dz; }

    // Everything the service knows about a pad: type, name, touchpad fingers, gyro/accel,
    // battery, Steam Input handle. Returns a zeroed state for an unplugged/out-of-range pad.
    const GamepadState& GetGamepadState(int pad) const;

    // Forwarded to GamepadService so scripts reach output through the same object they read
    // input from. low/high are the two motors, 0..1; durationSeconds <= 0 means until changed.
    void SetGamepadRumble(int pad, float low, float high, float durationSeconds);
    void StopGamepadRumble(int pad);
    void SetGamepadLightColor(int pad, int r, int g, int b);

    InputManager() = default;
    ~InputManager() override = default;

    InputManager* Copy() override;
    InputManager* Copy(Container* container) override;


private:
    // Snapshots the live pad buttons into both buffers. Used on Copy() so entering play mode
    // while holding a button does not fire a phantom press on the runtime container's first
    // frame.
    void SeedGamepadButtons();

    static constexpr int kPadButtonCount = static_cast<int>(GamepadButton::Count);

    bool m_padButtons[kMaxGamepads][kPadButtonCount]     = {};
    bool m_padButtonsPrev[kMaxGamepads][kPadButtonCount] = {};

    // 0.15 is the usual starting point for a modern stick; DualSense units in particular vary
    // enough that a smaller value lets worn pads drift.
    float m_stickDeadzone = 0.15f;

    std::unordered_map<int, bool> m_keyStates;
    std::unordered_set<int> m_keysJustPressedQueue;
    std::unordered_set<int> m_keysJustPressed;

    std::unordered_map<int, bool> m_mouseButtonStates;
    std::unordered_set<int> m_mouseButtonsJustPressedQueue;
    std::unordered_set<int> m_mouseButtonsJustPressed;

    RenderView* m_mouseSourceView = nullptr;   // non-owning; see SetMouseScreenPosition
    glm::vec2   m_mouseScreenPos  = {0.0f, 0.0f};
};
