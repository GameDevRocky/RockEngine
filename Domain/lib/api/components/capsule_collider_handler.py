from rock_engine.components import capsule_collider_module
from .collider_handler import Collider

class CapsuleCollider(Collider):
    _type_name = "CapsuleCollider"

    def __init__(self, obj_id=None):
        super().__init__(obj_id)

    @property
    def radius(self) -> float:
        return capsule_collider_module.get_radius(self._gameobject_id)

    @radius.setter
    def radius(self, value: float):
        capsule_collider_module.set_radius(self._gameobject_id, float(value))

    @property
    def height(self) -> float:
        return capsule_collider_module.get_height(self._gameobject_id)

    @height.setter
    def height(self, value: float):
        capsule_collider_module.set_height(self._gameobject_id, float(value))
