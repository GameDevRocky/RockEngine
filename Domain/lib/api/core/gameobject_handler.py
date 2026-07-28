from __future__ import annotations

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
    def tag(self):
        return gameobject_module.get_tag(self.id)

    @tag.setter
    def tag(self, val):
        gameobject_module.set_tag(self.id, str(val))

    @property
    def active(self):
        return gameobject_module.get_active(self.id)

    @active.setter
    def active(self, val):
        gameobject_module.set_active(self.id, bool(val))

    @property
    def name(self):
        return gameobject_module.get_name(self.id)

    @name.setter
    def name(self, val):
        gameobject_module.set_name(self.id, str(val))
    
    @property 
    def transform(self):
        # Defer import to avoid circular dependency during module import
        from ..components.transform_handler import Transform
        return self.get_component(Transform)

    def add_component(self, cls: Type[T]) -> Optional[T]:
        type_name = getattr(cls, '_type_name', None)
        if type_name:
            comp_id = gameobject_module.add_component(self.id, type_name)
            if not comp_id:
                return None
            # Multi-instance components (joints) are addressed by their own id, so
            # return a handler bound to THIS instance rather than a cached
            # first-match from get_component.
            if getattr(cls, '_addressed_by_component_id', False):
                return cls(self.id, comp_id)
        return self.get_component(cls)

    def get_component(self, cls: Type[T]) -> Optional[T]:
        # cls is the Class type (e.g., Transform)
        if cls in self._comp_cache:
            return self._comp_cache[cls]

        if getattr(cls, '_addressed_by_component_id', False):
            # First instance of that type, bound to its own id. Use get_components
            # when a GameObject may hold several.
            comps = self.get_components(cls)
            comp = comps[0] if comps else None
            if comp is None:
                return None
        else:
            comp = cls(self.id)
        self._comp_cache[cls] = comp
        return comp

    def get_components(self, cls: Type[T]) -> list:
        """Every component of this type on the object, each bound to its own id.

        get_component() can only ever name one instance per type; use this for
        anything a GameObject can carry several of, such as joints.
        """
        type_name = getattr(cls, '_type_name', None)
        if not type_name:
            return []
        ids = gameobject_module.get_component_ids(self.id, type_name)
        if getattr(cls, '_addressed_by_component_id', False):
            return [cls(self.id, comp_id) for comp_id in ids]
        # Handlers that predate component-id addressing only take the object id.
        return [cls(self.id) for _ in ids]


    def destroy(self):
        _GO_REGISTRY.pop(self.id, None)
        gameobject_module.shut_down(self.id)
