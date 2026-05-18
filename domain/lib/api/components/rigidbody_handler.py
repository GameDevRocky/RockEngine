
from rock_engine.components import rigidbody_module
from ...utils.re_math import Vector2
from .component_handler import Component

class Rigidbody(Component):
    _type_name = "RigidBody"

    def __init__(self, obj_id= None):
        super().__init__(obj_id)

    def apply_force(self, force : Vector2):
        force = Vector2(force)
        rigidbody_module.apply_force(self._gameobject_id, force.x, force.y)
        
    def apply_impulse(self, force : Vector2):
        force = Vector2(force)
        rigidbody_module.apply_impulse(self._gameobject_id, force.x, force.y)

    @property
    def velocity(self) -> Vector2:
        return Vector2(rigidbody_module.get_velocity(self._gameobject_id))

    @velocity.setter
    def velocity(self, velocity : Vector2):
        velocity = Vector2(velocity)
        rigidbody_module.set_velocity(self._gameobject_id, velocity.x, velocity.y)