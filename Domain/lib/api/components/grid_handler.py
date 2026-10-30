from rock_engine.components import grid_module

from .component_handler import Component
from ...utils.re_math import Vector2

class Grid(Component):
    _type_name = "Grid"
    __slots__ = ()


    @property
    def size(self):
        return Vector2._bound(grid_module.get_size, grid_module.set_size, self._gameobject_id)

    @size.setter
    def size(self, size : Vector2):
        size = Vector2(size)
        grid_module.set_size(self._gameobject_id, size.x, size.y)
    
