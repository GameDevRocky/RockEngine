
from rock_engine.components import rigidbody_module
from ...utils.re_math import Vector2
from .component_handler import Component

class Rigidbody(Component):
    def __init__(self, obj_id= None):
        super().__init__(obj_id)

    def apply_force(self, force : Vector2):
        force = Vector2(force)
        rigidbody_module.apply_force(self._gameobject_id, force.x, force.y)
        
    def apply_impulse(self, force : Vector2):
        force = Vector2(force)
        rigidbody_module.apply_impulse(self._gameobject_id, force.x, force.y)
        