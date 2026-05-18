from rock_engine.components import circle_collider_module
from .collider_handler import Collider

class CircleCollider(Collider):
    _type_name = "CircleCollider"

    def __init__(self, obj_id=None):
        super().__init__(obj_id)

    @property
    def radius(self) -> float:
        return circle_collider_module.get_radius(self._gameobject_id)

    @radius.setter
    def radius(self, value: float):
        circle_collider_module.set_radius(self._gameobject_id, float(value))
