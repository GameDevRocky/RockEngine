from rock_engine.core import gameobject_module
from typing import Type, TypeVar, Optional

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
        return gameobject_module.get_active(self.id)

    @active.setter
    def active(self, val):
        gameobject_module.set_active(self.id, val)

    @property
    def name(self):
        return gameobject_module.get_name(self.id)

    @name.setter
    def name(self, val):
        gameobject_module.set_name(self.id, val)
    
    @property 
    def transform(self):
        # Defer import to avoid circular dependency during module import
        from ..components.transform_handler import Transform
        return self.get_component(Transform)

    def get_component(self, cls: Type[T]) -> Optional[T]:
        # cls is the Class type (e.g., Transform)
        if cls in self._comp_cache:
            return self._comp_cache[cls]
        
        comp = cls(self.id)
        self._comp_cache[cls] = comp
        return comp