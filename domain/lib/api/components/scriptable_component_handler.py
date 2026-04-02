from .component_handler import Component
from .collider_handler import Collider


class ScriptableComponent(Component):
    def __init__(self, obj_id=None):
        super().__init__(obj_id)

    def on_collision_enter(self, other: Collider):
        pass
    def on_collision_exit(self, other: Collider):
        pass
    def on_trigger_enter(self, other: Collider):
        pass
    def on_trigger_exit(self, other: Collider):
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
        