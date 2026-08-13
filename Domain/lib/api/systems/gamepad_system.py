from rock_engine.systems import gamepad_module
from ...utils.re_math import Vector2


class PadButton:
    """Buttons are named by POSITION so the same constant means the same physical
    button on every controller. PlayStation labels are provided as aliases below --
    CROSS is SOUTH, they are the same value, use whichever reads better.

    WATCH OUT FOR 'X'. The two vendors disagree, and the aliases below follow the
    hardware, not intuition:
        PadButton.X     == WEST  == Xbox X == Square on PlayStation
        PadButton.CROSS == SOUTH == Xbox A == the X-looking button on PlayStation
    If you mean the button a PlayStation player calls X, you want CROSS."""

    SOUTH = 0
    EAST = 1
    WEST = 2
    NORTH = 3
    BACK = 4            # Share / Create on PlayStation, View on Xbox
    GUIDE = 5           # PS button / Xbox button
    START = 6           # Options on PlayStation, Menu on Xbox
    LEFT_STICK = 7      # L3
    RIGHT_STICK = 8     # R3
    LEFT_SHOULDER = 9   # L1 / LB
    RIGHT_SHOULDER = 10  # R1 / RB
    DPAD_UP = 11
    DPAD_DOWN = 12
    DPAD_LEFT = 13
    DPAD_RIGHT = 14
    MISC1 = 15          # DualSense mic button / Xbox Series share
    RIGHT_PADDLE1 = 16  # DualSense Edge / Xbox Elite back paddles
    LEFT_PADDLE1 = 17
    RIGHT_PADDLE2 = 18
    LEFT_PADDLE2 = 19
    TOUCHPAD = 20       # the touchpad CLICK; see Gamepad.touch() for the surface itself

    # --- PlayStation aliases ---
    CROSS = SOUTH
    CIRCLE = EAST
    SQUARE = WEST
    TRIANGLE = NORTH
    SHARE = BACK
    CREATE = BACK
    OPTIONS = START
    PS = GUIDE
    L1 = LEFT_SHOULDER
    R1 = RIGHT_SHOULDER
    L3 = LEFT_STICK
    R3 = RIGHT_STICK
    MIC = MISC1

    # --- Xbox aliases ---
    A = SOUTH
    B = EAST
    X = WEST
    Y = NORTH
    VIEW = BACK
    MENU = START
    LB = LEFT_SHOULDER
    RB = RIGHT_SHOULDER


class PadAxis:
    """Raw axis indices. Sticks are [-1, 1] with +Y pointing UP; triggers are [0, 1].
    Prefer Gamepad.left_stick()/right_stick() for sticks -- those apply the radial
    deadzone. These raw values do not."""

    LEFT_X = 0
    LEFT_Y = 1
    RIGHT_X = 2
    RIGHT_Y = 3
    LEFT_TRIGGER = 4    # L2 / LT
    RIGHT_TRIGGER = 5   # R2 / RT


class PadType:
    UNKNOWN = 0
    STANDARD = 1
    XBOX360 = 2
    XBOX_ONE = 3
    PS3 = 4
    PS4 = 5
    PS5 = 6
    SWITCH_PRO = 7
    SWITCH_JOYCON_LEFT = 8
    SWITCH_JOYCON_RIGHT = 9
    SWITCH_JOYCON_PAIR = 10
    GAMECUBE = 11


class Gamepad:
    """Controller input. Every call takes an optional pad index so local multiplayer
    is just a loop -- pad 0 is the default and is what a single-player game wants.

    Indices are stable for as long as a controller stays connected: unplugging pad 0
    does not renumber pad 1.

    Note that gamepad input is gated on application focus. While the RockEngine window
    is in the background every button reads released and every axis reads zero, so a
    game left in play mode does not respond while you work elsewhere.
    """

    # --- Connection ---------------------------------------------------------------
    @staticmethod
    def count():
        """How many controllers are connected."""
        return int(gamepad_module.get_count())

    @staticmethod
    def is_connected(pad=0):
        return bool(gamepad_module.is_connected(pad))

    @staticmethod
    def name(pad=0):
        return gamepad_module.get_name(pad)

    @staticmethod
    def type(pad=0):
        """One of the PadType constants. Use it to pick button glyphs -- show a
        Cross prompt on a PS4/PS5 pad and an A prompt on an Xbox one."""
        return int(gamepad_module.get_type(pad))

    @staticmethod
    def is_playstation(pad=0):
        return Gamepad.type(pad) in (PadType.PS3, PadType.PS4, PadType.PS5)

    @staticmethod
    def steam_handle(pad=0):
        """Non-zero when Steam Input owns this controller, meaning the game is seeing
        Steam's virtual pad rather than the physical device. Remapping is Steam's job
        in that case, so a game should not offer its own rebinding UI."""
        return int(gamepad_module.get_steam_handle(pad))

    # --- Buttons ------------------------------------------------------------------
    @staticmethod
    def is_button_down(button, pad=0):
        """True every frame the button is held."""
        return bool(gamepad_module.is_button_down(pad, button))

    @staticmethod
    def is_button_pressed(button, pad=0):
        """True only on the frame the button is first pressed."""
        return bool(gamepad_module.is_button_pressed(pad, button))

    @staticmethod
    def is_button_released(button, pad=0):
        """True only on the frame the button is released."""
        return bool(gamepad_module.is_button_released(pad, button))

    # --- Sticks and triggers -------------------------------------------------------
    @staticmethod
    def left_stick(pad=0):
        """Vector2 in the unit circle, radial deadzone applied. +Y is up."""
        return Vector2(gamepad_module.get_stick(pad, 0))

    @staticmethod
    def right_stick(pad=0):
        return Vector2(gamepad_module.get_stick(pad, 1))

    @staticmethod
    def axis(axis, pad=0):
        """Raw axis value, no deadzone. See PadAxis."""
        return float(gamepad_module.get_axis(pad, axis))

    @staticmethod
    def left_trigger(pad=0):
        """L2 / LT as 0..1."""
        return float(gamepad_module.get_axis(pad, PadAxis.LEFT_TRIGGER))

    @staticmethod
    def right_trigger(pad=0):
        """R2 / RT as 0..1."""
        return float(gamepad_module.get_axis(pad, PadAxis.RIGHT_TRIGGER))

    @staticmethod
    def get_deadzone():
        return float(gamepad_module.get_stick_deadzone())

    @staticmethod
    def set_deadzone(value):
        """Radial deadzone for left_stick/right_stick. Defaults to 0.15."""
        gamepad_module.set_stick_deadzone(float(value))

    # --- Touchpad (DualShock 4 / DualSense) -----------------------------------------
    @staticmethod
    def has_touchpad(pad=0):
        return int(gamepad_module.get_touchpad_count(pad)) > 0

    @staticmethod
    def touch(finger=0, pad=0):
        """Returns (down, Vector2(x, y), pressure) for one of the two fingers the
        PlayStation touchpad tracks. x/y are normalized 0..1 with +Y up.
        The touchpad CLICK is a normal button -- PadButton.TOUCHPAD."""
        down, x, y, pressure = gamepad_module.get_touch(pad, finger)
        return bool(down), Vector2((x, y)), float(pressure)

    # --- Motion (DualShock 4 / DualSense) --------------------------------------------
    @staticmethod
    def has_gyro(pad=0):
        return bool(gamepad_module.has_gyro(pad))

    @staticmethod
    def has_accelerometer(pad=0):
        return bool(gamepad_module.has_accel(pad))

    @staticmethod
    def gyro(pad=0):
        """Angular velocity as an (x, y, z) tuple in radians/sec."""
        return gamepad_module.get_gyro(pad)

    @staticmethod
    def accelerometer(pad=0):
        """Acceleration as an (x, y, z) tuple in m/s^2, gravity included -- a pad
        resting on a table reads about 9.8 on one axis rather than zero."""
        return gamepad_module.get_accel(pad)

    # --- Power ------------------------------------------------------------------------
    @staticmethod
    def battery_percent(pad=0):
        """0..100, or -1 when unknown or wired. Do not read -1 or 0 as 'flat'."""
        return int(gamepad_module.get_battery_percent(pad))

    # --- Output -----------------------------------------------------------------------
    @staticmethod
    def has_rumble(pad=0):
        return bool(gamepad_module.has_rumble(pad))

    @staticmethod
    def set_rumble(low=0.0, high=0.0, duration=0.0, pad=0):
        """Start both motors. low is the heavy/low-frequency motor, high the light
        one, each 0..1. duration is in seconds; 0 means run until changed or stopped.

        Rumble is cut automatically when the app loses focus and when play mode ends,
        so a script cannot leave a controller buzzing on the desk."""
        gamepad_module.set_rumble(pad, float(low), float(high), float(duration))

    @staticmethod
    def stop_rumble(pad=0):
        gamepad_module.stop_rumble(pad)

    @staticmethod
    def has_light(pad=0):
        return bool(gamepad_module.has_light(pad))

    @staticmethod
    def set_light_color(r, g, b, pad=0):
        """Set the DualShock 4 lightbar / DualSense player LED colour. Each channel
        is 0..255. No-op on controllers without an LED."""
        gamepad_module.set_light_color(pad, int(r), int(g), int(b))
