from rock_engine.components import sprite_renderer_module
from ...utils.re_math import Vector2
from .component_handler import Component
from ..rendering.sprite_handler import Sprite

class SpriteRenderer(Component):
    _type_name = "SpriteRenderer"

    def __init__(self, obj_id= None):
        super().__init__(obj_id)

    @property
    def sprite(self) -> Sprite:
        sprite_id = sprite_renderer_module.get_sprite_id(self._gameobject_id)
        return Sprite(sprite_id) if sprite_id else None

    @sprite.setter
    def sprite(self, value: Sprite):
        if value is not None:
            sprite_renderer_module.set_sprite_id(self._gameobject_id, value.id)

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
        return sprite_renderer_module.get_color(self._gameobject_id)

    @color.setter
    def color(self, value):
        r, g, b, a = value
        sprite_renderer_module.set_color(self._gameobject_id, float(r), float(g), float(b), float(a))

    
    @property
    def uv_offset(self) -> Vector2:
        return Vector2(sprite_renderer_module.get_uv_offset(self._gameobject_id))

    @uv_offset.setter
    def uv_offset(self, value):
        value = Vector2(value)
        sprite_renderer_module.set_uv_offset(self._gameobject_id, value.x, value.y)

    @property
    def uv_scale(self) -> Vector2:
        return Vector2(sprite_renderer_module.get_uv_scale(self._gameobject_id))

    @uv_scale.setter
    def uv_scale(self, value):
        value = Vector2(value)
        sprite_renderer_module.set_uv_scale(self._gameobject_id, value.x, value.y)


    def set_uniform(self, name: str, value):
        if isinstance(value, str):
            sprite_renderer_module.set_uniform_texture(self._gameobject_id, name, value)
        elif isinstance(value, (int, float)):
            sprite_renderer_module.set_uniform_float(self._gameobject_id, name, float(value))
        else:
            v = tuple(value)
            if len(v) == 2:
                sprite_renderer_module.set_uniform_vec2(self._gameobject_id, name, float(v[0]), float(v[1]))
            elif len(v) == 3:
                sprite_renderer_module.set_uniform_vec3(self._gameobject_id, name, float(v[0]), float(v[1]), float(v[2]))
            elif len(v) == 4:
                sprite_renderer_module.set_uniform_vec4(self._gameobject_id, name, float(v[0]), float(v[1]), float(v[2]), float(v[3]))

    def remove_uniform(self, name: str):
        sprite_renderer_module.remove_uniform(self._gameobject_id, name)

    @property
    def visible(self) -> bool:
        return sprite_renderer_module.get_visible(self._gameobject_id)

    @visible.setter
    def visible(self, val: bool):
        sprite_renderer_module.set_visible(self._gameobject_id, bool(val))

