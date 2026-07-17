from __future__ import annotations
from rock_engine.components import base_component_module
from ..core.gameobject_handler import get_gameobject, GameObject
from typing import TYPE_CHECKING, Type, TypeVar, Optional

# Type variables for generic helpers
T = TypeVar('T')

# Imports used only for type checking to avoid runtime cycles
if TYPE_CHECKING:
    from .transform_handler import Transform

class Component:
    def __init__(self, obj_id=None):
        self._gameobject_id = obj_id
        self._component_id = None # Should be set by C++ or lookup

    @property
    def id(self):
        """The id of the GameObject this component is attached to. Component
        handlers are always constructed with their owning object's id, so this
        is what a `field : <ComponentType>` reference stores and round-trips
        through (mirrors Sprite/Material `.id`)."""
        return self._gameobject_id

    @property
    def gameobject(self) -> GameObject:
        # Use the central function to ensure identity is consistent
        return get_gameobject(self._gameobject_id)
    
    @property 
    def transform(self) -> Transform:
        return self.gameobject.transform

    @property 
    def enabled(self) -> bool:
        if not self._component_id: return True
        return base_component_module.get_enabled(self._component_id)
    
    @enabled.setter
    def enabled(self, val : bool):
        if isinstance(val, bool) and self._component_id:
            base_component_module.set_enabled(self._component_id, val)
    
    def get_component(self, cls: Type[T]) -> Optional[T]:
        # Generic helper: returns Optional of requested component type
        return self.gameobject.get_component(cls)