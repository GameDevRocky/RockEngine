from .component_handler import Component
from .collider_handler import Collider
from rock_engine.core import gameobject_module


class ScriptableComponent(Component):
    def __init__(self, obj_id=None):
        super().__init__(obj_id)

    def instantiate(self, name: str = "GameObject"):
        """Create a new GameObject in the same scene as this script's object."""
        from ..core.gameobject_handler import get_gameobject
        new_id = gameobject_module.instantiate(self._gameobject_id, name)
        if not new_id:
            return None
        return get_gameobject(new_id)

    def on_collision_enter(self, other: Collider):
        pass
    def on_collision_exit(self, other: Collider):
        pass
    def on_trigger_enter(self, other: Collider):
        pass
    def on_trigger_exit(self, other: Collider):
        pass
    def on_shutdown(self):
        pass

    def handle_collision_enter(self, id):
        other = Collider(id)
        self.on_collision_enter(other)
       
    def handle_collision_exit(self, id):
        other = Collider(id)
        self.on_collision_exit(other)
        
    def handle_trigger_enter(self, id):
        other = Collider(id)
        self.on_trigger_enter(other)
        
    def handle_trigger_exit(self, id ):
        other = Collider(id)
        self.on_trigger_exit(other)
        