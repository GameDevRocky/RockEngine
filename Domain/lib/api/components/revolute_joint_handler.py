from rock_engine.components import revolute_joint_module
from .joint_handler import Joint


class RevoluteJoint(Joint):
    """A hinge: pins two bodies at a point and lets them rotate about it.

    All angles are in degrees.
    """

    _type_name = "RevoluteJoint"

    @property
    def target_angle(self) -> float:
        return revolute_joint_module.get_target_angle(self._component_id)

    @target_angle.setter
    def target_angle(self, value: float):
        revolute_joint_module.set_target_angle(self._component_id, float(value))

    @property
    def enable_spring(self) -> bool:
        return revolute_joint_module.get_enable_spring(self._component_id)

    @enable_spring.setter
    def enable_spring(self, value: bool):
        revolute_joint_module.set_enable_spring(self._component_id, bool(value))

    @property
    def hertz(self) -> float:
        return revolute_joint_module.get_hertz(self._component_id)

    @hertz.setter
    def hertz(self, value: float):
        revolute_joint_module.set_hertz(self._component_id, float(value))

    @property
    def damping_ratio(self) -> float:
        return revolute_joint_module.get_damping_ratio(self._component_id)

    @damping_ratio.setter
    def damping_ratio(self, value: float):
        revolute_joint_module.set_damping_ratio(self._component_id, float(value))

    @property
    def enable_limit(self) -> bool:
        return revolute_joint_module.get_enable_limit(self._component_id)

    @enable_limit.setter
    def enable_limit(self, value: bool):
        revolute_joint_module.set_enable_limit(self._component_id, bool(value))

    @property
    def lower_angle(self) -> float:
        return revolute_joint_module.get_lower_angle(self._component_id)

    @lower_angle.setter
    def lower_angle(self, value: float):
        revolute_joint_module.set_lower_angle(self._component_id, float(value))

    @property
    def upper_angle(self) -> float:
        return revolute_joint_module.get_upper_angle(self._component_id)

    @upper_angle.setter
    def upper_angle(self, value: float):
        revolute_joint_module.set_upper_angle(self._component_id, float(value))

    @property
    def enable_motor(self) -> bool:
        return revolute_joint_module.get_enable_motor(self._component_id)

    @enable_motor.setter
    def enable_motor(self, value: bool):
        revolute_joint_module.set_enable_motor(self._component_id, bool(value))

    @property
    def motor_speed(self) -> float:
        """Motor target speed, in degrees per second."""
        return revolute_joint_module.get_motor_speed(self._component_id)

    @motor_speed.setter
    def motor_speed(self, value: float):
        revolute_joint_module.set_motor_speed(self._component_id, float(value))

    @property
    def max_motor_torque(self) -> float:
        return revolute_joint_module.get_max_motor_torque(self._component_id)

    @max_motor_torque.setter
    def max_motor_torque(self, value: float):
        revolute_joint_module.set_max_motor_torque(self._component_id, float(value))

    @property
    def angle(self) -> float:
        """Live hinge angle in degrees. Read-only."""
        return revolute_joint_module.get_angle(self._component_id)
