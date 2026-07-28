from rock_engine.components import motor_joint_module
from ...utils.re_math import Vector2
from .joint_handler import Joint


class MotorJoint(Joint):
    """Drives the relative velocity between two bodies.

    With zero velocity it behaves like top-down friction; with a velocity it acts
    as a conveyor or a soft drive. Has no anchors or limits by nature.
    """

    _type_name = "MotorJoint"

    @property
    def linear_velocity(self) -> Vector2:
        """Target linear velocity, in pixels per second."""
        return Vector2(motor_joint_module.get_linear_velocity(self._component_id))

    @linear_velocity.setter
    def linear_velocity(self, value: Vector2):
        value = Vector2(value)
        motor_joint_module.set_linear_velocity(self._component_id, value.x, value.y)

    @property
    def max_velocity_force(self) -> float:
        return motor_joint_module.get_max_velocity_force(self._component_id)

    @max_velocity_force.setter
    def max_velocity_force(self, value: float):
        motor_joint_module.set_max_velocity_force(self._component_id, float(value))

    @property
    def angular_velocity(self) -> float:
        """Target angular velocity, in degrees per second."""
        return motor_joint_module.get_angular_velocity(self._component_id)

    @angular_velocity.setter
    def angular_velocity(self, value: float):
        motor_joint_module.set_angular_velocity(self._component_id, float(value))

    @property
    def max_velocity_torque(self) -> float:
        return motor_joint_module.get_max_velocity_torque(self._component_id)

    @max_velocity_torque.setter
    def max_velocity_torque(self, value: float):
        motor_joint_module.set_max_velocity_torque(self._component_id, float(value))

    @property
    def linear_hertz(self) -> float:
        return motor_joint_module.get_linear_hertz(self._component_id)

    @linear_hertz.setter
    def linear_hertz(self, value: float):
        motor_joint_module.set_linear_hertz(self._component_id, float(value))

    @property
    def linear_damping_ratio(self) -> float:
        return motor_joint_module.get_linear_damping_ratio(self._component_id)

    @linear_damping_ratio.setter
    def linear_damping_ratio(self, value: float):
        motor_joint_module.set_linear_damping_ratio(self._component_id, float(value))

    @property
    def max_spring_force(self) -> float:
        return motor_joint_module.get_max_spring_force(self._component_id)

    @max_spring_force.setter
    def max_spring_force(self, value: float):
        motor_joint_module.set_max_spring_force(self._component_id, float(value))

    @property
    def angular_hertz(self) -> float:
        return motor_joint_module.get_angular_hertz(self._component_id)

    @angular_hertz.setter
    def angular_hertz(self, value: float):
        motor_joint_module.set_angular_hertz(self._component_id, float(value))

    @property
    def angular_damping_ratio(self) -> float:
        return motor_joint_module.get_angular_damping_ratio(self._component_id)

    @angular_damping_ratio.setter
    def angular_damping_ratio(self, value: float):
        motor_joint_module.set_angular_damping_ratio(self._component_id, float(value))

    @property
    def max_spring_torque(self) -> float:
        return motor_joint_module.get_max_spring_torque(self._component_id)

    @max_spring_torque.setter
    def max_spring_torque(self, value: float):
        motor_joint_module.set_max_spring_torque(self._component_id, float(value))
