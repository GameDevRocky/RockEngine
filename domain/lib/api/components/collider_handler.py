from rock_engine.components import collider_module
from ...utils.re_math import Vector2
from .component_handler import Component

class Collider(Component):
    _type_name = "BoxCollider"

    def __init__(self, obj_id=None):
        super().__init__(obj_id)

    @property
    def center(self) -> Vector2:
        return Vector2(collider_module.get_center(self._gameobject_id))

    @center.setter
    def center(self, value: Vector2):
        collider_module.set_center(self._gameobject_id, value.x, value.y)

    @property
    def density(self) -> float:
        return collider_module.get_density(self._gameobject_id)

    @density.setter
    def density(self, value: float):
        collider_module.set_density(self._gameobject_id, float(value))

    @property
    def bounciness(self) -> float:
        return collider_module.get_bounciness(self._gameobject_id)

    @bounciness.setter
    def bounciness(self, value: float):
        collider_module.set_bounciness(self._gameobject_id, float(value))

    @property
    def friction(self) -> float:
        return collider_module.get_friction(self._gameobject_id)

    @friction.setter
    def friction(self, value: float):
        collider_module.set_friction(self._gameobject_id, float(value))

    @property
    def rolling_resistance(self) -> float:
        return collider_module.get_rolling_resistance(self._gameobject_id)

    @rolling_resistance.setter
    def rolling_resistance(self, value: float):
        collider_module.set_rolling_resistance(self._gameobject_id, float(value))

    @property
    def is_sensor(self) -> bool:
        return collider_module.get_is_sensor(self._gameobject_id)

    @is_sensor.setter
    def is_sensor(self, value: bool):
        collider_module.set_is_sensor(self._gameobject_id, bool(value))
