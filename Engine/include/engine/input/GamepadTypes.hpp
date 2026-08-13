#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include <string>

// Controller-agnostic gamepad vocabulary. Deliberately free of any SDL include: SDL is an
// implementation detail of GamepadService.cpp, and letting SDL_gamepad.h leak into engine
// headers would drag it into every binding and every script-facing translation unit.
//
// The button order mirrors SDL_GamepadButton exactly, so the mapping is a cast rather than a
// switch. GamepadService.cpp static_asserts that correspondence -- if an SDL upgrade ever
// reorders its enum, the build breaks there instead of silently swapping Circle for Cross.

// How many pads are tracked at once. Lives here rather than on GamepadService so InputManager
// can size its edge-state arrays without pulling the service (and its SDL-facing lifetime) in.
inline constexpr int kMaxGamepads = 8;

// Buttons are named by POSITION, not by label, which is the only naming that stays true across
// vendors. South is the bottom face button: Cross on PlayStation, A on Xbox, B on a Switch Pro.
// Domain/lib/api/systems/gamepad_system.py exposes CROSS/CIRCLE/SQUARE/TRIANGLE aliases so
// PlayStation-facing script code can read naturally without the engine having to pick a side.
enum class GamepadButton : int {
    South = 0,          // Cross     / A
    East,               // Circle    / B
    West,               // Square    / X
    North,              // Triangle  / Y
    Back,               // Share/Create (PS)   / View (Xbox)
    Guide,              // PS button / Xbox button
    Start,              // Options   / Menu
    LeftStick,          // L3
    RightStick,         // R3
    LeftShoulder,       // L1        / LB
    RightShoulder,      // R1        / RB
    DpadUp,
    DpadDown,
    DpadLeft,
    DpadRight,
    Misc1,              // DualSense mic button / Xbox Series share
    RightPaddle1,       // DualSense Edge RB    / Xbox Elite P1
    LeftPaddle1,        // DualSense Edge LB    / Xbox Elite P3
    RightPaddle2,       // DualSense Edge right Fn / Xbox Elite P2
    LeftPaddle2,        // DualSense Edge left Fn  / Xbox Elite P4
    Touchpad,           // DS4/DualSense touchpad CLICK (the touches themselves are below)
    Count
};

// Sticks are normalized to [-1, 1] with Y already flipped to point UP, which is what every
// caller in a 2D engine actually wants -- SDL reports Y-down. Triggers are [0, 1].
enum class GamepadAxis : int {
    LeftX = 0,
    LeftY,
    RightX,
    RightY,
    LeftTrigger,        // L2 / LT -- analog on every pad we care about
    RightTrigger,       // R2 / RT
    Count
};

// Reported so script/UI can show the right glyphs (Cross vs A) and so PlayStation-only
// features can be probed without string-matching the device name.
enum class GamepadType : int {
    Unknown = 0,
    Standard,
    Xbox360,
    XboxOne,
    PS3,
    PS4,
    PS5,
    NintendoSwitchPro,
    NintendoSwitchJoyConLeft,
    NintendoSwitchJoyConRight,
    NintendoSwitchJoyConPair,
    GameCube,
    Count
};

// One finger on the touchpad. x/y are normalized 0..1 across the pad surface with y already
// flipped to match the stick convention (up is +1), so a touchpad drag and a stick push agree
// on which way is up.
struct GamepadTouch {
    bool  down     = false;
    float x        = 0.0f;
    float y        = 0.0f;
    float pressure = 0.0f;
};

// Everything known about one pad for one frame. Plain data, copied wholesale -- there is no
// handle in here, so a caller can hold on to a snapshot without owning the device.
struct GamepadState {
    bool        connected = false;
    GamepadType type      = GamepadType::Unknown;
    std::string name;

    // Non-zero when Steam Input owns this pad. Worth surfacing because it changes what the
    // player sees: under Steam Input the physical DualSense is hidden and this is Steam's
    // virtual pad, so remapping is Steam's job, not the game's.
    std::uint64_t steamHandle = 0;

    bool  buttons[static_cast<int>(GamepadButton::Count)] = {};
    float axes[static_cast<int>(GamepadAxis::Count)]      = {};

    // --- Capabilities, probed once on connect ---
    bool hasRumble   = false;
    bool hasLED      = false;   // DS4 lightbar / DualSense player LEDs
    bool hasGyro     = false;
    bool hasAccel    = false;
    int  touchpadCount = 0;

    // --- PlayStation extras (present on DS4/DualSense; empty elsewhere) ---
    static constexpr int kMaxTouches = 2;
    GamepadTouch touches[kMaxTouches];

    glm::vec3 gyro  = { 0.0f, 0.0f, 0.0f };   // angular velocity, radians/sec
    glm::vec3 accel = { 0.0f, 0.0f, 0.0f };   // acceleration, m/s^2 (includes gravity)

    // -1 when unknown or wired -- callers must not treat 0 as "flat battery".
    int batteryPercent = -1;
};
