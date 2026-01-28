from rock_engine.components import sprite_renderer_module
from ...utils.re_math import Vector2
from .component_handler import Component

class SpriteRenderer(Component):
    def __init__(self, obj_id= None):
        super().__init__(obj_id)

    @property
    def transform(self):
        return self.gameobject.transform
    
    @property 
    def flipX(self):
        x, _ = sprite_renderer_module.get_flip(self._gameobject_id)
        return x

    @flipX.setter
    def flipX(self, val):
        _, y = sprite_renderer_module.get_flip(self._gameobject_id)
        sprite_renderer_module.set_flip(self._gameobject_id, val, y)

    @property
    def flipY(self):
        _, y = sprite_renderer_module.get_flip(self._gameobject_id)
        return y

    @flipY.setter
    def flipY(self, val):
        x, _ = sprite_renderer_module.get_flip(self._gameobject_id)
        sprite_renderer_module.set_flip(self._gameobject_id, x, val)

    @property
    def color(self) -> list:
        # Returns (r, g, b, a)
        return sprite_renderer_module.get_color(self._gameobject_id)

    @color.setter
    def color(self, value):
        r, g, b, a = value
        sprite_renderer_module.set_color(self._gameobject_id, float(r), float(g), float(b), float(a))

    
