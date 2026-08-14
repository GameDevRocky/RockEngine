#pragma once

#include <SDL3/SDL.h>

// SDL -> Qt input-code translation for the standalone player.
//
// WHY THIS EXISTS AT ALL, because it looks like pointless indirection until it bites:
//
// InputManager::SetKeyState takes a raw int, and the value it expects is a Qt::Key. Those
// values are not an internal detail -- they are baked into the PUBLIC scripting API in
// Domain/lib/api/systems/input_system.py, where Keys.ESCAPE is literally 0x01000000 and
// MouseButton.LEFT is literally 1 (Qt::LeftButton). Every game script ever written against
// this engine holds Qt numbers.
//
// SDL numbers those same keys completely differently: SDLK_ESCAPE is 0x1b, SDLK_LEFT is
// 0x40000050, and -- the easiest one to miss -- SDL reports letters as LOWERCASE ASCII
// ('a' == 0x61) while Qt uses uppercase (Qt::Key_A == 0x41). Feed SDL's codes straight into
// InputManager and the digits and punctuation keep working while every letter, arrow,
// modifier, function key and mouse button silently stops -- a bug that appears ONLY in
// shipped builds, which is the worst place for one to appear.
//
// So the player translates, and the scripting API is left alone. Introducing a proper
// engine-side keycode enum is the better long-term answer, but it is a breaking change to
// every existing user script and it is not this change.
namespace PlayerInput {

    // Returns the Qt::Key value for an SDL keycode, or -1 if we have no mapping.
    // An unmapped key is dropped rather than passed through: passing an untranslated code
    // would set state on whatever Qt key happened to share that number.
    int ToQtKey(SDL_Keycode sdlKey);

    // Returns the Qt::MouseButton value (Left=1, Right=2, Middle=4), or -1 if unmapped.
    int ToQtMouseButton(Uint8 sdlButton);

}
