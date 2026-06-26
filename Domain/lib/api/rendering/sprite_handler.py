from rock_engine.rendering import sprite_module
from ...utils.re_math import Vector2
import math


class Sprite:
    def __init__(self, sprite_id: str):
        self.id = sprite_id

    @property
    def uv_min(self) -> Vector2:
        return Vector2(sprite_module.get_uv_min(self.id))
    
    @uv_min.setter
    def uv_min(self, value : Vector2):
        value = Vector2(value)
        return sprite_module.set_uv_min(self.id, value.x, value.y)
    
    @property
    def uv_max(self) -> Vector2:
        return Vector2(sprite_module.get_uv_max(self.id))
    
    @uv_max.setter
    def uv_max(self, value : Vector2):
        value = Vector2(value)
        return sprite_module.set_uv_max(self.id, value.x, value.y)
    
    @property
    def pivot(self) -> Vector2:
        return Vector2(sprite_module.get_pivot(self.id))
    
    @pivot.setter
    def pivot(self, value : Vector2):
        value = Vector2(value)
        return sprite_module.set_pivot(self.id, value.x, value.y)
