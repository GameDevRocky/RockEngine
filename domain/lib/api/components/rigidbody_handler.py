
from rock_engine.components import rigidbody_module
from ...utils.re_math import Vector2
from .component_handler import Component

class Rigidbody(Component):
    _type_name = "RigidBody"

    DYNAMIC   = "Dynamic"
    KINEMATIC = "Kinematic"
    STATIC    = "Static"

    def __init__(self, obj_id= None):
        super().__init__(obj_id)

    def apply_force(self, force : Vector2):
        force = Vector2(force)
        rigidbody_module.apply_force(self._gameobject_id, force.x, force.y)
        
    def apply_impulse(self, force : Vector2):
        force = Vector2(force)
        rigidbody_module.apply_impulse(self._gameobject_id, force.x, force.y)

    def apply_torque(self, torque : float):
        torque = float(torque)
        rigidbody_module.apply_torque(self._gameobject_id, torque)
        
    def apply_angular_impulse(self, impulse : float):
        impulse = Vector2(impulse)
        rigidbody_module.apply_angular_impulse(self._gameobject_id, impulse)

    @property
    def velocity(self) -> Vector2:
        return Vector2(rigidbody_module.get_velocity(self._gameobject_id))

    @velocity.setter
    def velocity(self, velocity : Vector2):
        velocity = Vector2(velocity)
        rigidbody_module.set_velocity(self._gameobject_id, velocity.x, velocity.y)

    @property
    def use_gravity(self) -> bool:
        return rigidbody_module.get_use_gravity(self._gameobject_id)

    @use_gravity.setter
    def use_gravity(self, val : bool):
        val = bool(val)
        rigidbody_module.set_use_gravity(self._gameobject_id, val)

    @property
    def body_type(self) -> str:
        return rigidbody_module.get_body_type(self._gameobject_id)

    @body_type.setter
    def body_type(self, val : str):
        rigidbody_module.set_body_type(self._gameobject_id, val)

    @property
    def lock_rotation(self) -> bool:
        return rigidbody_module.get_lock_rotation(self._gameobject_id)

    @lock_rotation.setter
    def lock_rotation(self, val : bool):
        rigidbody_module.set_lock_rotation(self._gameobject_id, bool(val))