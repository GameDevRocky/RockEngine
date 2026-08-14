#include "engine/input/GamepadService.hpp"
#include "engine/debug/Console.hpp"

#include <SDL3/SDL.h>

#ifdef _WIN32
    // Pulled in only for CoInitializeEx -- SDL's headers do not expose it.
    //
    // NOMINMAX first: windows.h defines min/max as function-like macros, and the preprocessor
    // happily expands them inside std::min/std::clamp below (the :: does not protect them).
    // WIN32_LEAN_AND_MEAN trims the rest of the surface we have no use for.
    #define NOMINMAX
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    // WIN32_LEAN_AND_MEAN is exactly what drops the COM headers from windows.h, so the one
    // thing we came for has to be asked for by name.
    #include <objbase.h>
    // windows.h's CreateEvent macro rewrites every Observable::CreateEvent() parsed after it
    // into CreateEventA -- the link error documented on AudioEngine.cpp, pointing at unrelated
    // static Event initializers. The engine headers above are already parsed so this TU is
    // safe, but undo it anyway so adding an include later does not resurrect the bug.
    #undef CreateEvent
#endif

#include <algorithm>
#include <cmath>

// The engine enums are declared to mirror SDL's ordering so translation is a cast rather than
// a switch. That is only safe if the two actually agree, so pin it here: an SDL upgrade that
// reorders SDL_GamepadButton breaks the build on this line instead of silently swapping
// Circle for Cross at runtime.
static_assert(static_cast<int>(GamepadButton::South)        == SDL_GAMEPAD_BUTTON_SOUTH);
static_assert(static_cast<int>(GamepadButton::East)         == SDL_GAMEPAD_BUTTON_EAST);
static_assert(static_cast<int>(GamepadButton::West)         == SDL_GAMEPAD_BUTTON_WEST);
static_assert(static_cast<int>(GamepadButton::North)        == SDL_GAMEPAD_BUTTON_NORTH);
static_assert(static_cast<int>(GamepadButton::Back)         == SDL_GAMEPAD_BUTTON_BACK);
static_assert(static_cast<int>(GamepadButton::Guide)        == SDL_GAMEPAD_BUTTON_GUIDE);
static_assert(static_cast<int>(GamepadButton::Start)        == SDL_GAMEPAD_BUTTON_START);
static_assert(static_cast<int>(GamepadButton::LeftStick)    == SDL_GAMEPAD_BUTTON_LEFT_STICK);
static_assert(static_cast<int>(GamepadButton::RightStick)   == SDL_GAMEPAD_BUTTON_RIGHT_STICK);
static_assert(static_cast<int>(GamepadButton::LeftShoulder) == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
static_assert(static_cast<int>(GamepadButton::RightShoulder)== SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
static_assert(static_cast<int>(GamepadButton::DpadUp)       == SDL_GAMEPAD_BUTTON_DPAD_UP);
static_assert(static_cast<int>(GamepadButton::DpadDown)     == SDL_GAMEPAD_BUTTON_DPAD_DOWN);
static_assert(static_cast<int>(GamepadButton::DpadLeft)     == SDL_GAMEPAD_BUTTON_DPAD_LEFT);
static_assert(static_cast<int>(GamepadButton::DpadRight)    == SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
static_assert(static_cast<int>(GamepadButton::Misc1)        == SDL_GAMEPAD_BUTTON_MISC1);
static_assert(static_cast<int>(GamepadButton::Touchpad)     == SDL_GAMEPAD_BUTTON_TOUCHPAD);

static_assert(static_cast<int>(GamepadAxis::LeftX)        == SDL_GAMEPAD_AXIS_LEFTX);
static_assert(static_cast<int>(GamepadAxis::LeftY)        == SDL_GAMEPAD_AXIS_LEFTY);
static_assert(static_cast<int>(GamepadAxis::RightX)       == SDL_GAMEPAD_AXIS_RIGHTX);
static_assert(static_cast<int>(GamepadAxis::RightY)       == SDL_GAMEPAD_AXIS_RIGHTY);
static_assert(static_cast<int>(GamepadAxis::LeftTrigger)  == SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
static_assert(static_cast<int>(GamepadAxis::RightTrigger) == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);

static_assert(static_cast<int>(GamepadType::PS4) == SDL_GAMEPAD_TYPE_PS4);
static_assert(static_cast<int>(GamepadType::PS5) == SDL_GAMEPAD_TYPE_PS5);

namespace {
    // SDL reports sticks as Sint16. The negative end reaches -32768 but the positive end stops
    // at 32767, so dividing by 32767 and clamping is what actually yields a symmetric [-1, 1]
    // instead of a range that is one step longer downward than upward.
    float NormalizeAxis(Sint16 raw) {
        return std::clamp(static_cast<float>(raw) / 32767.0f, -1.0f, 1.0f);
    }

    float NormalizeTrigger(Sint16 raw) {
        return std::clamp(static_cast<float>(raw) / 32767.0f, 0.0f, 1.0f);
    }

    Uint16 ToMotor(float v) {
        return static_cast<Uint16>(std::clamp(v, 0.0f, 1.0f) * 65535.0f);
    }
}

GamepadService& GamepadService::Get() {
    static GamepadService instance;
    return instance;
}

GamepadService::~GamepadService() {
    Shutdown();
}

void GamepadService::EnsureInitialized() {
    if (m_initialized)
        return;

#ifdef _WIN32
    // Same defence AudioEngine::EnsureInitialized documents, for the same reason. SDL's Windows
    // joystick backends (WGI/DirectInput/RawInput) initialize COM on the calling thread, and if
    // anything claims COINIT_MULTITHREADED before Qt gets its apartment, Qt's OleInitialize()
    // fails with RPC_E_CHANGED_MODE and every editor drag-and-drop dies silently. Claiming STA
    // ourselves first means we agree with Qt rather than race it, which makes the call ordering
    // between audio, gamepads and Qt startup irrelevant. Do not "fix" this by reordering init.
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
#endif

    // Gamepad only. This pulls in JOYSTICK + EVENTS implicitly and nothing else -- notably not
    // VIDEO. Video IS compiled into our SDL build now (RockEnginePlayer needs a window), so
    // that separation is no longer enforced by the build; keep it here. In the editor Qt owns
    // every window, and this call is the editor's only contact with SDL. In the player,
    // PlayerApp has already brought VIDEO up and SDL_Init is refcounted, so this is a cheap
    // no-op that still leaves the gamepad subsystem correctly owned by this service.
    if (!SDL_Init(SDL_INIT_GAMEPAD)) {
        Console::Alert(std::string("GamepadService: SDL_Init failed: ") + SDL_GetError());
        return;
    }

    m_initialized = true;

    // Adopt anything already plugged in. SDL also queues an ADDED event for these, but the
    // queue is not drained until the first Update(), and a pad connected before startup should
    // be usable on frame one rather than frame two.
    int count = 0;
    if (SDL_JoystickID* ids = SDL_GetGamepads(&count)) {
        for (int i = 0; i < count; ++i)
            OpenPad(static_cast<std::uint32_t>(ids[i]));
        SDL_free(ids);
    }
}

void GamepadService::Shutdown() {
    if (!m_initialized)
        return;

    for (int i = 0; i < kMaxPads; ++i) {
        if (m_pads[i]) {
            // Motors latch in hardware -- a pad closed mid-rumble keeps buzzing until it is
            // physically unplugged, which outlives the process.
            SDL_RumbleGamepad(static_cast<SDL_Gamepad*>(m_pads[i]), 0, 0, 0);
            SDL_CloseGamepad(static_cast<SDL_Gamepad*>(m_pads[i]));
        }
        m_pads[i]        = nullptr;
        m_instanceIds[i] = 0;
        m_states[i]      = GamepadState{};
    }

    SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    m_initialized = false;
}

int GamepadService::SlotForInstance(std::uint32_t instanceId) const {
    for (int i = 0; i < kMaxPads; ++i) {
        if (m_pads[i] && m_instanceIds[i] == instanceId)
            return i;
    }
    return -1;
}

void GamepadService::OpenPad(std::uint32_t instanceId) {
    if (SlotForInstance(instanceId) >= 0)
        return;

    int slot = -1;
    for (int i = 0; i < kMaxPads; ++i) {
        if (!m_pads[i]) { slot = i; break; }
    }
    if (slot < 0)
        return;   // more pads than we track; ignore rather than evict a live one

    SDL_Gamepad* pad = SDL_OpenGamepad(static_cast<SDL_JoystickID>(instanceId));
    if (!pad) {
        Console::Alert(std::string("GamepadService: SDL_OpenGamepad failed: ") + SDL_GetError());
        return;
    }

    m_pads[slot]        = pad;
    m_instanceIds[slot] = instanceId;

    GamepadState& s = m_states[slot];
    s = GamepadState{};
    s.connected   = true;
    s.type        = static_cast<GamepadType>(SDL_GetGamepadType(pad));
    s.steamHandle = SDL_GetGamepadSteamHandle(pad);
    if (const char* name = SDL_GetGamepadName(pad))
        s.name = name;

    // Capabilities are properties in SDL3 rather than the SDL2-era Has* calls.
    SDL_PropertiesID props = SDL_GetGamepadProperties(pad);
    s.hasRumble = SDL_GetBooleanProperty(props, SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN, false);
    s.hasLED    = SDL_GetBooleanProperty(props, SDL_PROP_GAMEPAD_CAP_RGB_LED_BOOLEAN, false);

    s.touchpadCount = SDL_GetNumGamepadTouchpads(pad);

    // Motion sensors are off by default -- a DualSense streams them at 250Hz and SDL will not
    // pay that cost for a game that never asks. We ask once, here, so the data is simply there.
    s.hasGyro  = SDL_GamepadHasSensor(pad, SDL_SENSOR_GYRO);
    s.hasAccel = SDL_GamepadHasSensor(pad, SDL_SENSOR_ACCEL);
    if (s.hasGyro)
        SDL_SetGamepadSensorEnabled(pad, SDL_SENSOR_GYRO, true);
    if (s.hasAccel)
        SDL_SetGamepadSensorEnabled(pad, SDL_SENSOR_ACCEL, true);

    // Light up the player number so a 2-pad session is legible on the hardware itself.
    SDL_SetGamepadPlayerIndex(pad, slot);

    Console::Comment("Gamepad connected [" + std::to_string(slot) + "]: " + s.name);
}

void GamepadService::ClosePad(std::uint32_t instanceId) {
    const int slot = SlotForInstance(instanceId);
    if (slot < 0)
        return;

    Console::Comment("Gamepad disconnected [" + std::to_string(slot) + "]: " + m_states[slot].name);

    SDL_CloseGamepad(static_cast<SDL_Gamepad*>(m_pads[slot]));
    m_pads[slot]        = nullptr;
    m_instanceIds[slot] = 0;
    m_states[slot]      = GamepadState{};
}

void GamepadService::Update() {
    if (!m_initialized)
        return;

    // Drain the whole queue, not just the gamepad range. SDL owns no window here, so the only
    // traffic is joystick/gamepad, but an undrained queue grows without bound and SDL_PollEvent
    // is what pumps device state in the first place.
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_EVENT_GAMEPAD_ADDED:
                OpenPad(static_cast<std::uint32_t>(e.gdevice.which));
                break;
            case SDL_EVENT_GAMEPAD_REMOVED:
                ClosePad(static_cast<std::uint32_t>(e.gdevice.which));
                break;
            default:
                break;
        }
    }

    for (int i = 0; i < kMaxPads; ++i) {
        if (m_pads[i])
            ReadPad(i);
    }
}

void GamepadService::ReadPad(int slot) {
    SDL_Gamepad*  pad = static_cast<SDL_Gamepad*>(m_pads[slot]);
    GamepadState& s   = m_states[slot];

    for (int b = 0; b < static_cast<int>(GamepadButton::Count); ++b)
        s.buttons[b] = SDL_GetGamepadButton(pad, static_cast<SDL_GamepadButton>(b));

    s.axes[static_cast<int>(GamepadAxis::LeftX)]  = NormalizeAxis(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX));
    s.axes[static_cast<int>(GamepadAxis::RightX)] = NormalizeAxis(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTX));

    // Y is negated on the way in. SDL reports screen-space (down is positive); every caller in
    // a 2D engine with a Y-up world wants "push the stick up, get +1", and doing the flip once
    // here beats every script remembering to do it.
    s.axes[static_cast<int>(GamepadAxis::LeftY)]  = -NormalizeAxis(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY));
    s.axes[static_cast<int>(GamepadAxis::RightY)] = -NormalizeAxis(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTY));

    s.axes[static_cast<int>(GamepadAxis::LeftTrigger)]  = NormalizeTrigger(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER));
    s.axes[static_cast<int>(GamepadAxis::RightTrigger)] = NormalizeTrigger(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));

    // Touchpad: DS4 and DualSense both report two fingers on touchpad 0.
    if (s.touchpadCount > 0) {
        const int fingers = std::min(SDL_GetNumGamepadTouchpadFingers(pad, 0), GamepadState::kMaxTouches);
        for (int f = 0; f < GamepadState::kMaxTouches; ++f) {
            GamepadTouch& t = s.touches[f];
            if (f >= fingers) { t = GamepadTouch{}; continue; }

            bool  down = false;
            float x = 0.0f, y = 0.0f, p = 0.0f;
            if (SDL_GetGamepadTouchpadFinger(pad, 0, f, &down, &x, &y, &p)) {
                t.down     = down;
                t.x        = x;
                t.y        = 1.0f - y;   // flipped to agree with the stick convention
                t.pressure = p;
            } else {
                t = GamepadTouch{};
            }
        }
    }

    if (s.hasGyro) {
        float v[3] = {};
        if (SDL_GetGamepadSensorData(pad, SDL_SENSOR_GYRO, v, 3))
            s.gyro = { v[0], v[1], v[2] };
    }
    if (s.hasAccel) {
        float v[3] = {};
        if (SDL_GetGamepadSensorData(pad, SDL_SENSOR_ACCEL, v, 3))
            s.accel = { v[0], v[1], v[2] };
    }

    int percent = -1;
    SDL_GetGamepadPowerInfo(pad, &percent);
    s.batteryPercent = percent;
}

int GamepadService::GetPadCount() const {
    int n = 0;
    for (int i = 0; i < kMaxPads; ++i) {
        if (m_pads[i]) ++n;
    }
    return n;
}

bool GamepadService::IsConnected(int pad) const {
    if (pad < 0 || pad >= kMaxPads)
        return false;
    // Connection is reported honestly even when unfocused -- "is a pad plugged in" is not
    // input, and UI that shows a controller icon should not flicker when you alt-tab.
    return m_pads[pad] != nullptr;
}

const GamepadState& GamepadService::GetState(int pad) const {
    if (pad < 0 || pad >= kMaxPads || !m_pads[pad] || !m_focused)
        return m_zeroState;
    return m_states[pad];
}

void GamepadService::SetApplicationFocused(bool focused) {
    if (m_focused == focused)
        return;
    m_focused = focused;

    // Rumble is the one piece of state that outlives the gate: the motors are physical and keep
    // spinning on their own, so a pad buzzing at the moment you alt-tab would buzz forever.
    if (!focused)
        StopAllRumble();
}

void GamepadService::SetRumble(int pad, float low, float high, float durationSeconds) {
    if (pad < 0 || pad >= kMaxPads || !m_pads[pad] || !m_focused)
        return;

    const Uint32 ms = durationSeconds > 0.0f
                        ? static_cast<Uint32>(durationSeconds * 1000.0f)
                        : 0;   // 0 = until changed
    SDL_RumbleGamepad(static_cast<SDL_Gamepad*>(m_pads[pad]), ToMotor(low), ToMotor(high), ms);
}

void GamepadService::StopRumble(int pad) {
    if (pad < 0 || pad >= kMaxPads || !m_pads[pad])
        return;
    SDL_RumbleGamepad(static_cast<SDL_Gamepad*>(m_pads[pad]), 0, 0, 0);
}

void GamepadService::StopAllRumble() {
    for (int i = 0; i < kMaxPads; ++i)
        StopRumble(i);
}

void GamepadService::SetLightColor(int pad, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    if (pad < 0 || pad >= kMaxPads || !m_pads[pad])
        return;
    SDL_SetGamepadLED(static_cast<SDL_Gamepad*>(m_pads[pad]), r, g, b);
}

void GamepadService::SetPlayerIndex(int pad, int playerIndex) {
    if (pad < 0 || pad >= kMaxPads || !m_pads[pad])
        return;
    SDL_SetGamepadPlayerIndex(static_cast<SDL_Gamepad*>(m_pads[pad]), playerIndex);
}
