from rock_engine.components import wheel_joint_module
from .joint_handler import Joint


class WheelJoint(Joint):
    """A wheel that spins freely while sliding along a sprung axis -- vehicle
    suspension.

    `axis_angle` (degrees) is the suspension travel direction; 90 (the default)
    is the usual vertical suspension.
    """

    _type_name = "WheelJoint"

    @property
    def axis_angle(self) -> float:
        return wheel_joint_module.get_axis_angle(self._component_id)

    @axis_angle.setter
    def axis_angle(self, value: float):
        wheel_joint_module.set_axis_angle(self._component_id, float(value))

    @property
    def enable_spring(self) -> bool:
        return wheel_joint_module.get_enable_spring(self._component_id)

    @enable_spring.setter
    def enable_spring(self, value: bool):
        wheel_joint_module.set_enable_spring(self._component_id, bool(value))

    @property
    def hertz(self) -> float:
        return wheel_joint_module.get_hertz(self._component_id)

    @hertz.setter
    def hertz(self, value: float):
        wheel_joint_module.set_hertz(self._component_id, float(value))

    @property
    def damping_ratio(self) -> float:
        return wheel_joint_module.get_damping_ratio(self._component_id)

    @damping_ratio.setter
    def damping_ratio(self, value: float):
        wheel_joint_module.set_damping_ratio(self._component_id, float(value))

    @property
    def enable_limit(self) -> bool:
        return wheel_joint_module.get_enable_limit(self._component_id)

    @enable_limit.setter
    def enable_limit(self, value: bool):
        wheel_joint_module.set_enable_limit(self._component_id, bool(value))

    @property
    def lower_translation(self) -> float:
        return wheel_joint_module.get_lower_translation(self._component_id)

    @lower_translation.setter
    def lower_translation(self, value: float):
        wheel_joint_module.set_lower_translation(self._component_id, float(value))

    @property
    def upper_translation(self) -> float:
        return wheel_joint_module.get_upper_translation(self._component_id)

    @upper_translation.setter
    def upper_translation(self, value: float):
        wheel_joint_module.set_upper_translation(self._component_id, float(value))

    @property
    def enable_motor(self) -> bool:
        return wheel_joint_module.get_enable_motor(self._component_id)

    @enable_motor.setter
    def enable_motor(self, value: bool):
        wheel_joint_module.set_enable_motor(self._component_id, bool(value))

    @property
    def motor_speed(self) -> float:
        """Drive speed, in degrees per second."""
        return wheel_joint_module.get_motor_speed(self._component_id)

    @motor_speed.setter
    def motor_speed(self, value: float):
        wheel_joint_module.set_motor_speed(self._component_id, float(value))

    @property
    def max_motor_torque(self) -> float:
        return wheel_joint_module.get_max_motor_torque(self._component_id)

    @max_motor_torque.setter
    def max_motor_torque(self, value: float):
        wheel_joint_module.set_max_motor_torque(self._component_id, float(value))
