from rock_engine.components import transform_module
from ...utils.re_math import Vector2
from .component_handler import Component

class Transform(Component):

    def __init__(self, obj_id= None):
        super().__init__(obj_id)
        self.scale = self.scale

    @property
    def transform(self):
        return self

    @property
    def position(self) -> Vector2:
        return Vector2(transform_module.get_position(self._gameobject_id))

    @position.setter
    def position(self, value : Vector2):
        value = Vector2(value)
        transform_module.set_position(self._gameobject_id, float(value.x), float(value.y))

    @property
    def rotation(self):
        return transform_module.get_rotation(self._gameobject_id)

    @rotation.setter
    def rotation(self, value):
        transform_module.set_rotation(self._gameobject_id, float(value))

    @property
    def scale(self):
        return Vector2(transform_module.get_scale(self._gameobject_id))

    @scale.setter
    def scale(self, value):
        value = Vector2(value)
        transform_module.set_scale(self._gameobject_id, float(value.x), float(value.y))