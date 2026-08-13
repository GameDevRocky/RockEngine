#pragma once
#include "engine/input/GamepadTypes.hpp"
#include <cstdint>

// The physical controllers attached to the machine, and the one place SDL is spoken to.
//
// Lives OUTSIDE any Container, alongside Renderer/AssetManager/AudioEngine and for the same
// reason: a controller is hardware, and hardware has no per-world identity. There is one
// DualSense on the desk whether the editor container or the runtime container is ticking, and
// deep-copying it into play mode would mean two objects fighting over one device handle.
//
// InputManager (which IS per-Container) reads this each frame and derives the pressed/released
// edges, so scripts still get container-scoped input state while the device stays global. That
// split mirrors AudioEngine (global device) vs AudioSource (per-world component).
//
// Why SDL at all: Qt6 dropped QtGamepad and Engine must not depend on Qt regardless, so there
// was no cross-platform path to a pad. SDL is built here with SDL_VIDEO OFF (see
// External/CMakeLists.txt) -- it cannot create a window, and only SDL_INIT_GAMEPAD is ever
// initialized. Qt keeps owning every window.
class GamepadService {
public:
    static GamepadService& Get();

    // Starts SDL's gamepad subsystem and adopts any pad already plugged in. Idempotent.
    void EnsureInitialized();
    void Shutdown();
    bool IsInitialized() const { return m_initialized; }

    // Drains SDL's event queue (hotplug) and refreshes every open pad's state. Must run once
    // per frame from Engine::Update BEFORE the active Container ticks, so InputManager sees
    // this frame's data rather than last frame's.
    void Update();

    // Up to this many pads are tracked at once; extras are ignored rather than rotated in, so
    // a pad's index stays stable for as long as it is connected.
    static constexpr int kMaxPads = kMaxGamepads;

    // How many pads are connected right now. Slots can have holes (unplug pad 0 while pad 1
    // stays), so this is a count, not a bound -- iterate 0..kMaxPads and test IsConnected.
    int  GetPadCount() const;
    bool IsConnected(int pad) const;
    const GamepadState& GetState(int pad) const;    // zeroed if out of range/disconnected/unfocused

    // --- Focus gating ---------------------------------------------------------------------
    // A polled pad has no window focus concept: without this, a game left in play mode keeps
    // responding to stick input while you are working in another application. The editor
    // pushes Qt's application-activation state in (see Editor::Init). While unfocused every
    // GetState() returns zeroes and rumble is cut, but the devices stay open so nothing has to
    // be re-enumerated on the way back.
    void SetApplicationFocused(bool focused);
    bool IsApplicationFocused() const { return m_focused; }

    // --- Output ---------------------------------------------------------------------------
    // low = the heavy/low-frequency motor, high = the light/high-frequency one, both 0..1.
    // durationSeconds <= 0 means "until changed"; SDL stops the motors on its own otherwise.
    void SetRumble(int pad, float low, float high, float durationSeconds);
    void StopRumble(int pad);
    void StopAllRumble();

    // DS4 lightbar / DualSense player indicator. No-op on pads without an LED.
    void SetLightColor(int pad, std::uint8_t r, std::uint8_t g, std::uint8_t b);

    // Drives the player-number LEDs on pads that have them (DualSense, Xbox, Switch Pro).
    void SetPlayerIndex(int pad, int playerIndex);

private:
    GamepadService() = default;
    ~GamepadService();
    GamepadService(const GamepadService&) = delete;
    GamepadService& operator=(const GamepadService&) = delete;

    void OpenPad(std::uint32_t instanceId);
    void ClosePad(std::uint32_t instanceId);
    int  SlotForInstance(std::uint32_t instanceId) const;
    void ReadPad(int slot);

    // void* rather than SDL_Gamepad* so this header stays SDL-free -- see GamepadTypes.hpp.
    void*         m_pads[kMaxPads]      = {};
    std::uint32_t m_instanceIds[kMaxPads] = {};
    GamepadState  m_states[kMaxPads];

    // Handed back for any query that must not report live hardware: an unplugged slot, an
    // out-of-range index, or the whole service while the app is in the background.
    GamepadState m_zeroState;

    bool m_initialized = false;
    bool m_focused     = true;
};
