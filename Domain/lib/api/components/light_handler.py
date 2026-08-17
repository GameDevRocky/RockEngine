from rock_engine.components import light_module
from .component_handler import Component


class LightType:
    """Values match the C++ Light::LightType enum order."""
    POINT = 0
    SPOT = 1
    DIRECTIONAL = 2
    GLOBAL = 3


class Light(Component):
    """A 2D light. One component covers every light type via `type`, matching the
    engine side — switching Point to Spot is a routine authoring move and should
    not cost a delete plus re-add.

    Units are pixels at zoom 1 (world units == pixels), so `range = 300` reaches
    300px.
    """

    _type_name = "Light"

    def __init__(self, obj_id=None):
        super().__init__(obj_id)

    @property
    def type(self) -> int:
        """One of the LightType constants."""
        return light_module.get_type(self._gameobject_id)

    @type.setter
    def type(self, value: int):
        light_module.set_type(self._gameobject_id, int(value))

    @property
    def color(self) -> list:
        """[r, g, b, a], each 0..1."""
        return list(light_module.get_color(self._gameobject_id))

    @color.setter
    def color(self, value):
        r, g, b, a = value
        light_module.set_color(self._gameobject_id, float(r), float(g), float(b), float(a))

    @property
    def intensity(self) -> float:
        return light_module.get_intensity(self._gameobject_id)

    @intensity.setter
    def intensity(self, value: float):
        light_module.set_intensity(self._gameobject_id, float(value))

    # ── Point / Spot attenuation ─────────────────────────────────────────────
    @property
    def range(self) -> float:
        """Reach in pixels. Ignored by Directional and Global lights."""
        return light_module.get_range(self._gameobject_id)

    @range.setter
    def range(self, value: float):
        light_module.set_range(self._gameobject_id, float(value))

    @property
    def inner_radius(self) -> float:
        """Fraction of `range` (0..1) held at full brightness before falloff."""
        return light_module.get_inner_radius(self._gameobject_id)

    @inner_radius.setter
    def inner_radius(self, value: float):
        light_module.set_inner_radius(self._gameobject_id, float(value))

    @property
    def falloff(self) -> float:
        """Attenuation exponent: 1 is a linear-ish ramp, higher tightens the hotspot."""
        return light_module.get_falloff(self._gameobject_id)

    @falloff.setter
    def falloff(self, value: float):
        light_module.set_falloff(self._gameobject_id, float(value))

    # ── Spot cone ────────────────────────────────────────────────────────────
    @property
    def inner_angle(self) -> float:
        """Half-angle in degrees held at full strength."""
        return light_module.get_inner_angle(self._gameobject_id)

    @inner_angle.setter
    def inner_angle(self, value: float):
        light_module.set_inner_angle(self._gameobject_id, float(value))

    @property
    def outer_angle(self) -> float:
        """Half-angle in degrees where the cone reaches zero."""
        return light_module.get_outer_angle(self._gameobject_id)

    @outer_angle.setter
    def outer_angle(self, value: float):
        light_module.set_outer_angle(self._gameobject_id, float(value))

    # ── Normal-map shading ───────────────────────────────────────────────────
    @property
    def height(self) -> float:
        """Distance above the sprite plane. Sprites all sit at z=0, so without a
        Z offset every light vector is perfectly in-plane and a normal map
        produces no shading at all."""
        return light_module.get_height(self._gameobject_id)

    @height.setter
    def height(self, value: float):
        light_module.set_height(self._gameobject_id, float(value))

    @property
    def normal_influence(self) -> float:
        """0 ignores the surface normal (flat lighting), 1 is full N-dot-L."""
        return light_module.get_normal_influence(self._gameobject_id)

    @normal_influence.setter
    def normal_influence(self, value: float):
        light_module.set_normal_influence(self._gameobject_id, float(value))

    # ── Shadows ──────────────────────────────────────────────────────────────
    @property
    def cast_shadows(self) -> bool:
        return light_module.get_cast_shadows(self._gameobject_id)

    @cast_shadows.setter
    def cast_shadows(self, value: bool):
        light_module.set_cast_shadows(self._gameobject_id, bool(value))

    @property
    def shadow_strength(self) -> float:
        return light_module.get_shadow_strength(self._gameobject_id)

    @shadow_strength.setter
    def shadow_strength(self, value: float):
        light_module.set_shadow_strength(self._gameobject_id, float(value))

    # ── Derived ──────────────────────────────────────────────────────────────
    @property
    def world_direction(self) -> list:
        """Unit aim direction for Spot/Directional, as [x, y].

        Read-only by design: it comes from the owning Transform's world rotation
        and there is no authored direction field behind it. To aim a light,
        rotate its transform.
        """
        return list(light_module.get_world_direction(self._gameobject_id))
