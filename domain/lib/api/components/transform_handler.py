from rock_engine.components import transform_module
from ...utils.re_math import Vector2
from .component_handler import Component
from ..core.gameobject_handler import get_gameobject

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
    def rotation(self) -> float:
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

    @property
    def world_position(self) -> Vector2:
        return Vector2(transform_module.get_world_position(self._gameobject_id))

    @world_position.setter
    def world_position(self, value : Vector2):
        value = Vector2(value)
        transform_module.set_world_position(self._gameobject_id, float(value.x), float(value.y))

    @property
    def world_rotation(self):
        return transform_module.get_world_rotation(self._gameobject_id)

    @world_rotation.setter
    def world_rotation(self, value):
        transform_module.set_world_rotation(self._gameobject_id, float(value))

    @property
    def world_scale(self):
        return Vector2(transform_module.get_world_scale(self._gameobject_id))

    @world_scale.setter
    def world_scale(self, value):
        value = Vector2(value)
        transform_module.set_world_scale(self._gameobject_id, float(value.x), float(value.y))

    @property
    def parent(self):
        parent_id = transform_module.get_parent(self._gameobject_id)
        if parent_id is None:
            return None
        return get_gameobject(parent_id)

    @parent.setter
    def parent(self, value):
        if value is None:
            transform_module.set_parent(self._gameobject_id, None, True)
            return

        parent_id = getattr(value, "id", None)
        if parent_id is None:
            parent_id = getattr(value, "_gameobject_id", None)

        if parent_id is None:
            raise TypeError("Transform.parent expects a GameObject, Transform, or None")

        transform_module.set_parent(self._gameobject_id, parent_id, True)

    