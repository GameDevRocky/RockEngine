#include "PlayerInput.hpp"

#include <unordered_map>

namespace {

// Qt::Key values, spelled out rather than included, because Player must not link Qt.
// These are ABI-stable across Qt versions (they are part of Qt's public enum and Qt has
// never renumbered them) and they are the same numbers input_system.py hardcodes.
namespace QtKey {
    constexpr int Escape    = 0x01000000;
    constexpr int Tab       = 0x01000001;
    constexpr int Backtab   = 0x01000002;
    constexpr int Backspace = 0x01000003;
    constexpr int Return    = 0x01000004;
    constexpr int Enter     = 0x01000005;   // numpad enter
    constexpr int Insert    = 0x01000006;
    constexpr int Delete    = 0x01000007;
    constexpr int Pause     = 0x01000008;
    constexpr int Print     = 0x01000009;
    constexpr int SysReq    = 0x0100000a;
    constexpr int Home      = 0x01000010;
    constexpr int End       = 0x01000011;
    constexpr int Left      = 0x01000012;
    constexpr int Up        = 0x01000013;
    constexpr int Right     = 0x01000014;
    constexpr int Down      = 0x01000015;
    constexpr int PageUp    = 0x01000016;
    constexpr int PageDown  = 0x01000017;
    constexpr int Shift     = 0x01000020;
    constexpr int Control   = 0x01000021;
    constexpr int Meta      = 0x01000022;
    constexpr int Alt       = 0x01000023;
    constexpr int CapsLock  = 0x01000024;
    constexpr int NumLock   = 0x01000025;
    constexpr int ScrollLock= 0x01000026;
    constexpr int F1        = 0x01000030;   // F2..F12 are F1 + n
}

const std::unordered_map<SDL_Keycode, int>& KeyTable() {
    static const std::unordered_map<SDL_Keycode, int> table = [] {
        std::unordered_map<SDL_Keycode, int> t;

        // --- Letters -------------------------------------------------------------------
        // The subtle one. SDL delivers 'a'..'z' (0x61..0x7a); Qt::Key_A..Key_Z are
        // 0x41..0x5a. Without this every WASD script is dead in a build while still
        // working perfectly in the editor.
        for (SDL_Keycode k = SDLK_A; k <= SDLK_Z; ++k)
            t[k] = static_cast<int>(k) - 0x20;

        // --- Digits and ASCII punctuation ----------------------------------------------
        // These genuinely coincide between SDL and Qt (both are plain ASCII), but they are
        // listed explicitly rather than passed through so that "unmapped means dropped"
        // stays true for everything and there is no silent fallthrough path.
        for (SDL_Keycode k = SDLK_0; k <= SDLK_9; ++k)
            t[k] = static_cast<int>(k);

        const SDL_Keycode ascii[] = {
            SDLK_SPACE, SDLK_EXCLAIM, SDLK_DBLAPOSTROPHE, SDLK_HASH, SDLK_DOLLAR,
            SDLK_PERCENT, SDLK_AMPERSAND, SDLK_APOSTROPHE, SDLK_LEFTPAREN,
            SDLK_RIGHTPAREN, SDLK_ASTERISK, SDLK_PLUS, SDLK_COMMA, SDLK_MINUS,
            SDLK_PERIOD, SDLK_SLASH, SDLK_COLON, SDLK_SEMICOLON, SDLK_LESS,
            SDLK_EQUALS, SDLK_GREATER, SDLK_QUESTION, SDLK_AT, SDLK_LEFTBRACKET,
            SDLK_BACKSLASH, SDLK_RIGHTBRACKET, SDLK_CARET, SDLK_UNDERSCORE, SDLK_GRAVE,
        };
        for (SDL_Keycode k : ascii)
            t[k] = static_cast<int>(k);

        // --- Control keys: every one of these differs ------------------------------------
        t[SDLK_ESCAPE]    = QtKey::Escape;
        t[SDLK_TAB]       = QtKey::Tab;
        t[SDLK_BACKSPACE] = QtKey::Backspace;
        t[SDLK_RETURN]    = QtKey::Return;
        t[SDLK_KP_ENTER]  = QtKey::Enter;
        t[SDLK_INSERT]    = QtKey::Insert;
        t[SDLK_DELETE]    = QtKey::Delete;
        t[SDLK_PAUSE]     = QtKey::Pause;
        t[SDLK_PRINTSCREEN] = QtKey::Print;
        t[SDLK_HOME]      = QtKey::Home;
        t[SDLK_END]       = QtKey::End;
        t[SDLK_PAGEUP]    = QtKey::PageUp;
        t[SDLK_PAGEDOWN]  = QtKey::PageDown;

        t[SDLK_LEFT]  = QtKey::Left;
        t[SDLK_UP]    = QtKey::Up;
        t[SDLK_RIGHT] = QtKey::Right;
        t[SDLK_DOWN]  = QtKey::Down;

        // Qt collapses left/right modifiers into one code by default, and that is what
        // input_system.py exposes (Keys.SHIFT, not Keys.LSHIFT). Both sides map to it so
        // `Input.is_key_down(Keys.SHIFT)` answers for either physical key, matching what
        // the editor does with Qt's own events.
        t[SDLK_LSHIFT] = QtKey::Shift;   t[SDLK_RSHIFT] = QtKey::Shift;
        t[SDLK_LCTRL]  = QtKey::Control; t[SDLK_RCTRL]  = QtKey::Control;
        t[SDLK_LALT]   = QtKey::Alt;     t[SDLK_RALT]   = QtKey::Alt;
        t[SDLK_LGUI]   = QtKey::Meta;    t[SDLK_RGUI]   = QtKey::Meta;

        t[SDLK_CAPSLOCK]   = QtKey::CapsLock;
        t[SDLK_NUMLOCKCLEAR] = QtKey::NumLock;
        t[SDLK_SCROLLLOCK] = QtKey::ScrollLock;

        // --- Function keys ---------------------------------------------------------------
        // SDL's F1..F12 are contiguous and so are Qt's, so the offset walk is safe.
        for (int i = 0; i < 12; ++i)
            t[static_cast<SDL_Keycode>(SDLK_F1 + i)] = QtKey::F1 + i;

        // --- Numpad digits -----------------------------------------------------------------
        // Mapped onto the top-row digits deliberately: input_system.py has no separate
        // numpad constants, so a script asking for Keys.K_1 should hear either 1 key.
        const SDL_Keycode kp[] = {
            SDLK_KP_0, SDLK_KP_1, SDLK_KP_2, SDLK_KP_3, SDLK_KP_4,
            SDLK_KP_5, SDLK_KP_6, SDLK_KP_7, SDLK_KP_8, SDLK_KP_9,
        };
        for (int i = 0; i < 10; ++i)
            t[kp[i]] = 0x30 + i;

        return t;
    }();
    return table;
}

} // namespace

namespace PlayerInput {

int ToQtKey(SDL_Keycode sdlKey) {
    const auto& table = KeyTable();
    auto it = table.find(sdlKey);
    return (it == table.end()) ? -1 : it->second;
}

int ToQtMouseButton(Uint8 sdlButton) {
    // Qt::LeftButton=1, RightButton=2, MiddleButton=4 -- a bit flag enum, so these are NOT
    // sequential and SDL's 1/2/3 numbering does not line up on the middle button.
    switch (sdlButton) {
        case SDL_BUTTON_LEFT:   return 1;
        case SDL_BUTTON_RIGHT:  return 2;
        case SDL_BUTTON_MIDDLE: return 4;
        default:                return -1;
    }
}

} // namespace PlayerInput
