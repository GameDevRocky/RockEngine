import engine_api
from .math import Vector2
from typing import Type, TypeVar, Optional

# Global registry to ensure one ID = one Python Object
_GO_REGISTRY = {}
T = TypeVar('T')  # generic type placeholder

def get_gameobject(obj_id) -> GameObject:
    if obj_id not in _GO_REGISTRY:
        _GO_REGISTRY[obj_id] = GameObject(obj_id)
    return _GO_REGISTRY[obj_id]

class GameObject:
    def __init__(self, obj_id):
        self.id = obj_id
        self._comp_cache = {}

    @property
    def active(self):
        return engine_api.get_active(self.id)

    @active.setter
    def active(self, val):
        engine_api.set_active(self.id, val)
    
    @property 
    def transform(self):
        return self.get_component(Transform)

    def get_component(self, cls: Type[T]) -> Optional[T]:
        # cls is the Class type (e.g., Transform)
        if cls in self._comp_cache:
            return self._comp_cache[cls]
        
        comp = cls(self.id)
        self._comp_cache[cls] = comp
        return comp

class Component:
    def __init__(self, obj_id=None):
        self._gameobject_id = obj_id
        self._component_id = None # Should be set by C++ or lookup
    
    @property
    def gameobject(self) -> GameObject:
        # Use the central function to ensure identity is consistent
        return get_gameobject(self._gameobject_id)
    
    @property 
    def transform(self) -> 'Transform':
        return self.gameobject.transform

    @property 
    def enabled(self) -> bool:
        if not self._component_id: return True
        return engine_api.get_enabled(self._component_id)
    
    @enabled.setter
    def enabled(self, val : bool):
        if isinstance(val, bool) and self._component_id:
            engine_api.set_enabled(self._component_id, val)
    
    def get_component(self, cls: Type[T]) -> Optional[T]:
        return self.gameobject.get_component(cls)

class ScriptableComponent(Component):
    def __init__(self, obj_id=None):
        super().__init__(obj_id)
    

class Transform(Component):

    def __init__(self, obj_id= None):
        super().__init__(obj_id)

    @property
    def transform(self):
        return self

    @property
    def position(self) -> Vector2:
        return Vector2(engine_api.get_position(self._gameobject_id))

    @position.setter
    def position(self, value : Vector2):
        value = Vector2(value)
        engine_api.set_position(self._gameobject_id, float(value.x), float(value.y))

    @property
    def rotation(self):
        return engine_api.get_rotation(self._gameobject_id)

    @rotation.setter
    def rotation(self, value):
        engine_api.set_rotation(self._gameobject_id, float(value))

    @property
    def scale(self):
        return Vector2(engine_api.get_scale(self._gameobject_id))

    @scale.setter
    def scale(self, value):
        value = Vector2(value)
        engine_api.set_scale(self._gameobject_id, float(value.x), float(value.y))


class SpriteRenderer(Component):
    def __init__(self, obj_id= None):
        super().__init__(obj_id)

    @property
    def transform(self):
        return self.gameobject.transform
    
    @property 
    def flipX(self):
        x, _ = engine_api.get_flip(self._gameobject_id)
        return x

    @flipX.setter
    def flipX(self, val):
        _, y = engine_api.get_flip(self._gameobject_id)
        engine_api.set_flip(self._gameobject_id, val, y)

    @property
    def flipY(self):
        _, y = engine_api.get_flip(self._gameobject_id)
        return y

    @flipY.setter
    def flipY(self, val):
        x, _ = engine_api.get_flip(self._gameobject_id)
        engine_api.set_flip(self._gameobject_id, x, val)

    @property
    def color(self):
        # Returns (r, g, b, a)
        return engine_api.get_color(self._gameobject_id)

    @color.setter
    def color(self, value):
        r, g, b, a = value
        engine_api.set_color(self._gameobject_id, float(r), float(g), float(b), float(a))

    

    

