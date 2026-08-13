#include "engine/bindings/PythonBindings.hpp"
#include "engine/core/InputManager.hpp"
#include "Engine.hpp"

#include <tuple>

// Scripts reach gamepads through the same InputManager proxy the keyboard uses, so pad state is
// container-scoped for free (the Proxy re-resolves through GetActiveContainer on every call and
// therefore survives the play-mode container swap). Every entry point is null-guarded in the
// TimeBindings style rather than the InputBindings style -- a script touching input before a
// container exists should read "nothing pressed", not crash the interpreter.
void BindGamepad(pybind11::module_& m) {
    pybind11::module_ gamepad_module = m.def_submodule("gamepad_module", "Gamepad Bindings");

    // --- Connection -------------------------------------------------------------------------
    gamepad_module.def("get_count", []() -> int {
        return inputManager ? inputManager->GetGamepadCount() : 0;
    });

    gamepad_module.def("is_connected", [](int pad) -> bool {
        return inputManager ? inputManager->IsGamepadConnected(pad) : false;
    });

    gamepad_module.def("get_name", [](int pad) -> std::string {
        return inputManager ? inputManager->GetGamepadState(pad).name : std::string();
    });

    // Raw GamepadType ordinal; gamepad_system.py maps it to the PadType constants.
    gamepad_module.def("get_type", [](int pad) -> int {
        return inputManager ? static_cast<int>(inputManager->GetGamepadState(pad).type) : 0;
    });

    // Non-zero when Steam Input owns the pad -- the game is seeing Steam's virtual controller
    // rather than the physical one, and remapping is Steam's job.
    gamepad_module.def("get_steam_handle", [](int pad) -> std::uint64_t {
        return inputManager ? inputManager->GetGamepadState(pad).steamHandle : 0;
    });

    // --- Buttons ----------------------------------------------------------------------------
    gamepad_module.def("is_button_down", [](int pad, int button) -> bool {
        return inputManager
            ? inputManager->IsGamepadButtonDown(pad, static_cast<GamepadButton>(button))
            : false;
    });

    gamepad_module.def("is_button_pressed", [](int pad, int button) -> bool {
        return inputManager
            ? inputManager->IsGamepadButtonPressed(pad, static_cast<GamepadButton>(button))
            : false;
    });

    gamepad_module.def("is_button_released", [](int pad, int button) -> bool {
        return inputManager
            ? inputManager->IsGamepadButtonReleased(pad, static_cast<GamepadButton>(button))
            : false;
    });

    // --- Axes -------------------------------------------------------------------------------
    gamepad_module.def("get_axis", [](int pad, int axis) -> float {
        return inputManager
            ? inputManager->GetGamepadAxis(pad, static_cast<GamepadAxis>(axis))
            : 0.0f;
    });

    gamepad_module.def("get_stick", [](int pad, int stick) {
        if (!inputManager) return std::make_tuple(0.0f, 0.0f);
        const glm::vec2 v = inputManager->GetGamepadStick(pad, static_cast<GamepadStick>(stick));
        return std::make_tuple(v.x, v.y);
    });

    gamepad_module.def("get_stick_deadzone", []() -> float {
        return inputManager ? inputManager->GetStickDeadzone() : 0.0f;
    });

    gamepad_module.def("set_stick_deadzone", [](float dz) {
        if (inputManager) inputManager->SetStickDeadzone(dz);
    });

    // --- Touchpad (DualShock 4 / DualSense) ---------------------------------------------------
    gamepad_module.def("get_touchpad_count", [](int pad) -> int {
        return inputManager ? inputManager->GetGamepadState(pad).touchpadCount : 0;
    });

    // (down, x, y, pressure) for one finger. x/y normalized 0..1 with y up.
    gamepad_module.def("get_touch", [](int pad, int finger) {
        if (!inputManager || finger < 0 || finger >= GamepadState::kMaxTouches)
            return std::make_tuple(false, 0.0f, 0.0f, 0.0f);
        const GamepadTouch& t = inputManager->GetGamepadState(pad).touches[finger];
        return std::make_tuple(t.down, t.x, t.y, t.pressure);
    });

    // --- Motion (DualShock 4 / DualSense) -----------------------------------------------------
    gamepad_module.def("has_gyro", [](int pad) -> bool {
        return inputManager ? inputManager->GetGamepadState(pad).hasGyro : false;
    });

    gamepad_module.def("has_accel", [](int pad) -> bool {
        return inputManager ? inputManager->GetGamepadState(pad).hasAccel : false;
    });

    gamepad_module.def("get_gyro", [](int pad) {
        if (!inputManager) return std::make_tuple(0.0f, 0.0f, 0.0f);
        const glm::vec3& g = inputManager->GetGamepadState(pad).gyro;
        return std::make_tuple(g.x, g.y, g.z);
    });

    gamepad_module.def("get_accel", [](int pad) {
        if (!inputManager) return std::make_tuple(0.0f, 0.0f, 0.0f);
        const glm::vec3& a = inputManager->GetGamepadState(pad).accel;
        return std::make_tuple(a.x, a.y, a.z);
    });

    // --- Power ---------------------------------------------------------------------------------
    // -1 means unknown or wired; callers must not read 0 as "flat".
    gamepad_module.def("get_battery_percent", [](int pad) -> int {
        return inputManager ? inputManager->GetGamepadState(pad).batteryPercent : -1;
    });

    // --- Output --------------------------------------------------------------------------------
    gamepad_module.def("has_rumble", [](int pad) -> bool {
        return inputManager ? inputManager->GetGamepadState(pad).hasRumble : false;
    });

    gamepad_module.def("set_rumble", [](int pad, float low, float high, float duration) {
        if (inputManager) inputManager->SetGamepadRumble(pad, low, high, duration);
    });

    gamepad_module.def("stop_rumble", [](int pad) {
        if (inputManager) inputManager->StopGamepadRumble(pad);
    });

    gamepad_module.def("has_light", [](int pad) -> bool {
        return inputManager ? inputManager->GetGamepadState(pad).hasLED : false;
    });

    gamepad_module.def("set_light_color", [](int pad, int r, int g, int b) {
        if (inputManager) inputManager->SetGamepadLightColor(pad, r, g, b);
    });
}
