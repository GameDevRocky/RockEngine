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
    def __init__(self, obj_id=None, component_id=None):
        self._gameobject_id = obj_id
        # The component's OWN id. Needed by anything a GameObject can hold more
        # than one of (joints, colliders), since a GameObject id alone cannot say
        # which instance you mean. Populated by GameObject.add_component /
        # get_components; None when the handler was built by type lookup alone.
        self._component_id = component_id

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

    def _resolve_component_id(self):
        """This component's own id, looked up by type on first use.

        Handlers built by type (the get_component path) know only their owning
        GameObject, but base_component_module addresses components by their own
        id. Resolving lazily here is what makes `enabled` work for those handlers
        instead of silently no-opping.
        """
        if self._component_id:
            return self._component_id
        type_name = getattr(type(self), '_type_name', None)
        if not type_name or not self._gameobject_id:
            return None
        from rock_engine.core import gameobject_module
        ids = gameobject_module.get_component_ids(self._gameobject_id, type_name)
        if ids:
            self._component_id = ids[0]
        return self._component_id

    @property
    def enabled(self) -> bool:
        comp_id = self._resolve_component_id()
        if not comp_id: return True
        return base_component_module.get_enabled(comp_id)

    @enabled.setter
    def enabled(self, val : bool):
        if not isinstance(val, bool):
            return
        comp_id = self._resolve_component_id()
        if comp_id:
            base_component_module.set_enabled(comp_id, val)
    
    def get_component(self, cls: Type[T]) -> Optional[T]:
        # Generic helper: returns Optional of requested component type
        return self.gameobject.get_component(cls)