from rock_engine.components import trail_module
from ...utils.re_math import Vector4
from ...utils.animation_curve import AnimationCurve
from .component_handler import Component
from ..rendering.sprite_handler import Sprite


class TrailRenderer(Component):
    """Scripting handle for a trail ribbon. Thin proxy over ``trail_module`` (C++).

    ``width_curve`` is the one property that does not behave like the rest: an
    :class:`AnimationCurve` is a value, so reading it gives you a **copy**.
    Mutating that copy does not write back -- assign it again.

        curve = trail.width_curve
        curve.add_key(0.5, 0.8)
        trail.width_curve = curve

    Typical use is toggling emission rather than adding/removing the component::

        trail.emitting = False      # tail keeps fading out on its own
        trail.clear()               # or drop it instantly, e.g. after a teleport
    """
    _type_name = "TrailRenderer"

    def __init__(self, obj_id=None):
        super().__init__(obj_id)

    # ── Shape ────────────────────────────────────────────────────────────────
    @property
    def time(self) -> float:
        """Seconds a recorded point survives before it expires."""
        return trail_module.get_time(self._gameobject_id)

    @time.setter
    def time(self, v):
        trail_module.set_time(self._gameobject_id, float(v))

    @property
    def min_vertex_distance(self) -> float:
        """How far the object must move before another point is recorded."""
        return trail_module.get_min_vertex_distance(self._gameobject_id)

    @min_vertex_distance.setter
    def min_vertex_distance(self, v):
        trail_module.set_min_vertex_distance(self._gameobject_id, float(v))

    @property
    def width_curve(self) -> AnimationCurve:
        """Width along the ribbon, sampled 0 (head) to 1 (tail). Returns a copy."""
        return trail_module.get_width_curve(self._gameobject_id)

    @width_curve.setter
    def width_curve(self, curve: AnimationCurve):
        trail_module.set_width_curve(self._gameobject_id, curve)

    @property
    def width_multiplier(self) -> float:
        """Pixels the curve is scaled by, so the curve itself can stay 0..1."""
        return trail_module.get_width_multiplier(self._gameobject_id)

    @width_multiplier.setter
    def width_multiplier(self, v):
        trail_module.set_width_multiplier(self._gameobject_id, float(v))

    # ── Appearance ───────────────────────────────────────────────────────────
    @property
    def start_color(self) -> Vector4:
        return Vector4(trail_module.get_start_color(self._gameobject_id))

    @start_color.setter
    def start_color(self, value):
        r, g, b, a = value
        trail_module.set_start_color(self._gameobject_id, float(r), float(g), float(b), float(a))

    @property
    def end_color(self) -> Vector4:
        return Vector4(trail_module.get_end_color(self._gameobject_id))

    @end_color.setter
    def end_color(self, value):
        r, g, b, a = value
        trail_module.set_end_color(self._gameobject_id, float(r), float(g), float(b), float(a))

    @property
    def sprite(self) -> Sprite:
        sprite_id = trail_module.get_sprite(self._gameobject_id)
        return Sprite(sprite_id) if sprite_id else None

    @sprite.setter
    def sprite(self, value: Sprite):
        trail_module.set_sprite(self._gameobject_id, value.id if value is not None else "")

    @property
    def material(self) -> str:
        return trail_module.get_material(self._gameobject_id)

    @material.setter
    def material(self, value):
        trail_module.set_material(self._gameobject_id, str(value))

    # ── Emission ─────────────────────────────────────────────────────────────
    @property
    def emitting(self) -> bool:
        """False stops new points; the existing tail still ages out."""
        return trail_module.get_emitting(self._gameobject_id)

    @emitting.setter
    def emitting(self, v):
        trail_module.set_emitting(self._gameobject_id, bool(v))

    @property
    def autodestruct(self) -> bool:
        """Destroy the GameObject once emitting stopped and the trail emptied."""
        return trail_module.get_autodestruct(self._gameobject_id)

    @autodestruct.setter
    def autodestruct(self, v):
        trail_module.set_autodestruct(self._gameobject_id, bool(v))

    # ── Sorting ──────────────────────────────────────────────────────────────
    @property
    def sorting_layer(self) -> str:
        return trail_module.get_sorting_layer(self._gameobject_id)

    @sorting_layer.setter
    def sorting_layer(self, v):
        trail_module.set_sorting_layer(self._gameobject_id, str(v))

    @property
    def sorting_order(self) -> int:
        return trail_module.get_sorting_order(self._gameobject_id)

    @sorting_order.setter
    def sorting_order(self, v):
        trail_module.set_sorting_order(self._gameobject_id, int(v))

    # ── State ────────────────────────────────────────────────────────────────
    @property
    def point_count(self) -> int:
        """Recorded points currently alive. Read-only."""
        return trail_module.get_point_count(self._gameobject_id)

    def clear(self):
        """Drop the whole history at once -- use after teleporting an object so
        it does not drag a ribbon across the screen behind it."""
        trail_module.clear(self._gameobject_id)
