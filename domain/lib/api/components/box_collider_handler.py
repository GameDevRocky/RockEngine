from rock_engine.components import box_collider_module
from ...utils.re_math import Vector2
from .collider_handler import Collider

class BoxCollider(Collider):
    _type_name = "BoxCollider"

    def __init__(self, obj_id=None):
        super().__init__(obj_id)

    @property
    def size(self) -> Vector2:
        return Vector2(box_collider_module.get_size(self._gameobject_id))

    @size.setter
    def size(self, value: Vector2):
        box_collider_module.set_size(self._gameobject_id, value.x, value.y)
