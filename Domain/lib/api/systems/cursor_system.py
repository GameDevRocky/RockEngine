from rock_engine.systems import cursor_module

from ...utils.re_math import Vector2


class CursorLockState:
    """How the native pointer is constrained by the active game view."""

    NONE = 0
    CONFINED = 1
    LOCKED = 2


class _Cursor:
    """Process cursor controls, modeled after Unity's Cursor API.

    Custom art uses a RockEngine Sprite, so atlas sub-sprites work without a separate cursor
    asset type. Hotspots are pixel offsets from the cropped sprite's top-left corner.
    """

    @property
    def visible(self) -> bool:
        return bool(cursor_module.is_visible())

    @visible.setter
    def visible(self, value: bool):
        cursor_module.set_visible(bool(value))

    @property
    def lock_state(self) -> int:
        return int(cursor_module.get_lock_state())

    @lock_state.setter
    def lock_state(self, value: int):
        cursor_module.set_lock_state(int(value))

    @property
    def sprite_id(self) -> str:
        return str(cursor_module.get_sprite_id())

    @property
    def hotspot(self) -> Vector2:
        return Vector2(cursor_module.get_hotspot())

    def set_sprite(self, sprite, hotspot=Vector2(0, 0)):
        """Use a Sprite as the native cursor, or pass None to restore the system cursor."""
        hotspot = Vector2(hotspot)
        sprite_id = "" if sprite is None else sprite.id
        cursor_module.set_sprite(sprite_id, hotspot.x, hotspot.y)

    @staticmethod
    def reset_sprite():
        cursor_module.reset_sprite()

    @staticmethod
    def reset():
        """Restore visibility, unlock the pointer, and restore the system cursor."""
        cursor_module.reset()


Cursor = _Cursor()
