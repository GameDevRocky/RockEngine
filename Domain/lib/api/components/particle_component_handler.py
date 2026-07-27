from rock_engine.components import particle_module
from ...utils.re_math import Vector2, Vector4
from .component_handler import Component
from ..rendering.sprite_handler import Sprite


class ParticleComponent(Component):
    """Scripting handle for a GPU particle emitter. Thin proxy over
    ``particle_module`` (C++). Enum-valued fields use the nested int constants
    below, mirroring the editor dropdowns."""
    _type_name = "ParticleComponent"

    class Shape:
        POINT, CIRCLE, BOX, CONE = 0, 1, 2, 3

    class Space:
        LOCAL, WORLD = 0, 1

    class Blend:
        ALPHA, ADDITIVE = 0, 1

    def __init__(self, obj_id=None):
        super().__init__(obj_id)

    # ── Emission ─────────────────────────────────────────────────────────────
    @property
    def emission_rate(self) -> float:
        return particle_module.get_emission_rate(self._gameobject_id)

    @emission_rate.setter
    def emission_rate(self, v):
        particle_module.set_emission_rate(self._gameobject_id, float(v))

    @property
    def max_particles(self) -> int:
        return particle_module.get_max_particles(self._gameobject_id)

    @max_particles.setter
    def max_particles(self, v):
        particle_module.set_max_particles(self._gameobject_id, int(v))

    @property
    def duration(self) -> float:
        return particle_module.get_duration(self._gameobject_id)

    @duration.setter
    def duration(self, v):
        particle_module.set_duration(self._gameobject_id, float(v))

    @property
    def looping(self) -> bool:
        return particle_module.get_looping(self._gameobject_id)

    @looping.setter
    def looping(self, v):
        particle_module.set_looping(self._gameobject_id, bool(v))

    @property
    def start_burst(self) -> int:
        return particle_module.get_start_burst(self._gameobject_id)

    @start_burst.setter
    def start_burst(self, v):
        particle_module.set_start_burst(self._gameobject_id, int(v))

    # ── Lifetime / motion ── (min,max as Vector2: x=min, y=max)
    @property
    def lifetime(self) -> Vector2:
        return Vector2(particle_module.get_lifetime(self._gameobject_id))

    @lifetime.setter
    def lifetime(self, value):
        value = Vector2(value)
        particle_module.set_lifetime(self._gameobject_id, value.x, value.y)

    @property
    def speed(self) -> Vector2:
        return Vector2(particle_module.get_speed(self._gameobject_id))

    @speed.setter
    def speed(self, value):
        value = Vector2(value)
        particle_module.set_speed(self._gameobject_id, value.x, value.y)

    @property
    def direction(self) -> float:
        return particle_module.get_direction(self._gameobject_id)

    @direction.setter
    def direction(self, v):
        particle_module.set_direction(self._gameobject_id, float(v))

    @property
    def spread(self) -> float:
        return particle_module.get_spread(self._gameobject_id)

    @spread.setter
    def spread(self, v):
        particle_module.set_spread(self._gameobject_id, float(v))

    @property
    def gravity(self) -> Vector2:
        return Vector2(particle_module.get_gravity(self._gameobject_id))

    @gravity.setter
    def gravity(self, value):
        value = Vector2(value)
        particle_module.set_gravity(self._gameobject_id, value.x, value.y)

    @property
    def damping(self) -> float:
        return particle_module.get_damping(self._gameobject_id)

    @damping.setter
    def damping(self, v):
        particle_module.set_damping(self._gameobject_id, float(v))

    @property
    def space(self) -> int:
        return particle_module.get_space(self._gameobject_id)

    @space.setter
    def space(self, v):
        particle_module.set_space(self._gameobject_id, int(v))

    # ── Shape ────────────────────────────────────────────────────────────────
    @property
    def shape(self) -> int:
        return particle_module.get_shape(self._gameobject_id)

    @shape.setter
    def shape(self, v):
        particle_module.set_shape(self._gameobject_id, int(v))

    @property
    def shape_size(self) -> Vector2:
        return Vector2(particle_module.get_shape_size(self._gameobject_id))

    @shape_size.setter
    def shape_size(self, value):
        value = Vector2(value)
        particle_module.set_shape_size(self._gameobject_id, value.x, value.y)

    @property
    def cone_angle(self) -> float:
        return particle_module.get_cone_angle(self._gameobject_id)

    @cone_angle.setter
    def cone_angle(self, v):
        particle_module.set_cone_angle(self._gameobject_id, float(v))

    # ── Appearance ───────────────────────────────────────────────────────────
    @property
    def sprite(self) -> Sprite:
        sprite_id = particle_module.get_sprite_id(self._gameobject_id)
        return Sprite(sprite_id) if sprite_id else None

    @sprite.setter
    def sprite(self, value: Sprite):
        if value is not None:
            particle_module.set_sprite_id(self._gameobject_id, value.id)

    @property
    def flipX(self) -> bool:
        x, _ = particle_module.get_flip(self._gameobject_id)
        return x

    @flipX.setter
    def flipX(self, val: bool):
        _, y = particle_module.get_flip(self._gameobject_id)
        particle_module.set_flip(self._gameobject_id, bool(val), y)

    @property
    def flipY(self) -> bool:
        _, y = particle_module.get_flip(self._gameobject_id)
        return y

    @flipY.setter
    def flipY(self, val: bool):
        x, _ = particle_module.get_flip(self._gameobject_id)
        particle_module.set_flip(self._gameobject_id, x, bool(val))

    @property
    def start_color(self) -> Vector4:
        return Vector4(particle_module.get_start_color(self._gameobject_id))

    @start_color.setter
    def start_color(self, value):
        r, g, b, a = value
        particle_module.set_start_color(self._gameobject_id, float(r), float(g), float(b), float(a))

    @property
    def end_color(self) -> Vector4:
        return Vector4(particle_module.get_end_color(self._gameobject_id))

    @end_color.setter
    def end_color(self, value):
        r, g, b, a = value
        particle_module.set_end_color(self._gameobject_id, float(r), float(g), float(b), float(a))

    @property
    def start_size(self) -> float:
        return particle_module.get_start_size(self._gameobject_id)

    @start_size.setter
    def start_size(self, v):
        particle_module.set_start_size(self._gameobject_id, float(v))

    @property
    def end_size(self) -> float:
        return particle_module.get_end_size(self._gameobject_id)

    @end_size.setter
    def end_size(self, v):
        particle_module.set_end_size(self._gameobject_id, float(v))

    @property
    def blend_mode(self) -> int:
        return particle_module.get_blend_mode(self._gameobject_id)

    @blend_mode.setter
    def blend_mode(self, v):
        particle_module.set_blend_mode(self._gameobject_id, int(v))

    @property
    def sorting_layer(self) -> str:
        """Sorting layer name. Emitters share the sprite sorting model, so this
        places the effect among the sprite layers rather than always on top."""
        return particle_module.get_sorting_layer(self._gameobject_id)

    @sorting_layer.setter
    def sorting_layer(self, v):
        particle_module.set_sorting_layer(self._gameobject_id, str(v))

    @property
    def sorting_order(self) -> int:
        return particle_module.get_sorting_order(self._gameobject_id)

    @sorting_order.setter
    def sorting_order(self, v):
        particle_module.set_sorting_order(self._gameobject_id, int(v))

    # ── Actions ──────────────────────────────────────────────────────────────
    def emit_burst(self, count: int):
        """Spawn ``count`` particles immediately (explosions, puffs)."""
        particle_module.emit_burst(self._gameobject_id, int(count))
