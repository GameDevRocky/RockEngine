import engine_api
from  .math import Vector2
class Keys:
    # --- Standard Keys (ASCII) ---
    SPACE        = 32
    EXCLAM       = 0x21
    QUOTE_DBL    = 0x22
    HASH         = 0x23
    DOLLAR       = 0x24
    PERCENT      = 0x25
    AMPERSAND    = 0x26
    APOSTROPHE   = 0x27
    PAREN_LEFT   = 0x28
    PAREN_RIGHT  = 0x29
    ASTERISK     = 0x2a
    PLUS         = 0x2b
    COMMA        = 0x2c
    MINUS        = 0x2d
    PERIOD       = 0x2e
    SLASH        = 0x2f

    # --- Numbers ---
    K_0 = 0x30
    K_1 = 0x31
    K_2 = 0x32
    K_3 = 0x33
    K_4 = 0x34
    K_5 = 0x35
    K_6 = 0x36
    K_7 = 0x37
    K_8 = 0x38
    K_9 = 0x39

    # --- Alphabet (Uppercase) ---
    A = 0x41
    B = 0x42
    C = 0x43
    D = 0x44
    E = 0x45
    F = 0x46
    G = 0x47
    H = 0x48
    I = 0x49
    J = 0x4a
    K = 0x4b
    L = 0x4c
    M = 0x4d
    N = 0x4e
    O = 0x4f
    P = 0x50
    Q = 0x51
    R = 0x52
    S = 0x53
    T = 0x54
    U = 0x55
    V = 0x56
    W = 0x57
    X = 0x58
    Y = 0x59
    Z = 0x5a

    # --- Special Control Keys ---
    ESCAPE       = 0x01000000
    TAB          = 0x01000001
    BACKTAB      = 0x01000002
    BACKSPACE    = 0x01000003
    RETURN       = 0x01000004
    ENTER        = 0x01000005 # Numpad Enter
    INSERT       = 0x01000006
    DELETE       = 0x01000007
    PAUSE        = 0x01000008
    PRINT        = 0x01000009
    SYS_REQ      = 0x0100000a

    # --- Arrow Keys ---
    LEFT         = 0x01000012
    UP           = 0x01000013
    RIGHT        = 0x01000014
    DOWN         = 0x01000015

    # --- Modifier Keys ---
    SHIFT        = 0x01000020
    CONTROL      = 0x01000021
    META         = 0x01000022 # Windows/Cmd Key
    ALT          = 0x01000023
    CAPS_LOCK    = 0x01000024
    NUM_LOCK     = 0x01000025
    SCROLL_LOCK  = 0x01000026

    # --- Function Keys ---
    F1  = 0x01000030
    F2  = 0x01000031
    F3  = 0x01000032
    F4  = 0x01000033
    F5  = 0x01000034
    F6  = 0x01000035
    F7  = 0x01000036
    F8  = 0x01000037
    F9  = 0x01000038
    F10 = 0x01000039
    F11 = 0x0100003a
    F12 = 0x0100003b

class Input:
    @staticmethod
    def is_key_down(keycode):
        return bool(engine_api.is_key_down(keycode))
    
    @staticmethod
    def get_mouse_pos():
        return Vector2(engine_api.get_mouse_pos())

