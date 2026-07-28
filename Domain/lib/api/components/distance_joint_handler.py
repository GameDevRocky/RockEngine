from rock_engine.components import distance_joint_module
from .joint_handler import Joint


class DistanceJoint(Joint):
    """Holds two anchors a set distance apart -- ropes, tethers, springs."""

    _type_name = "DistanceJoint"

    @property
    def length(self) -> float:
        return distance_joint_module.get_length(self._component_id)

    @length.setter
    def length(self, value: float):
        distance_joint_module.set_length(self._component_id, float(value))

    @property
    def enable_spring(self) -> bool:
        return distance_joint_module.get_enable_spring(self._component_id)

    @enable_spring.setter
    def enable_spring(self, value: bool):
        distance_joint_module.set_enable_spring(self._component_id, bool(value))

    @property
    def hertz(self) -> float:
        return distance_joint_module.get_hertz(self._component_id)

    @hertz.setter
    def hertz(self, value: float):
        distance_joint_module.set_hertz(self._component_id, float(value))

    @property
    def damping_ratio(self) -> float:
        return distance_joint_module.get_damping_ratio(self._component_id)

    @damping_ratio.setter
    def damping_ratio(self, value: float):
        distance_joint_module.set_damping_ratio(self._component_id, float(value))

    @property
    def lower_spring_force(self) -> float:
        return distance_joint_module.get_lower_spring_force(self._component_id)

    @lower_spring_force.setter
    def lower_spring_force(self, value: float):
        distance_joint_module.set_lower_spring_force(self._component_id, float(value))

    @property
    def upper_spring_force(self) -> float:
        return distance_joint_module.get_upper_spring_force(self._component_id)

    @upper_spring_force.setter
    def upper_spring_force(self, value: float):
        distance_joint_module.set_upper_spring_force(self._component_id, float(value))

    @property
    def enable_limit(self) -> bool:
        return distance_joint_module.get_enable_limit(self._component_id)

    @enable_limit.setter
    def enable_limit(self, value: bool):
        distance_joint_module.set_enable_limit(self._component_id, bool(value))

    @property
    def min_length(self) -> float:
        return distance_joint_module.get_min_length(self._component_id)

    @min_length.setter
    def min_length(self, value: float):
        distance_joint_module.set_min_length(self._component_id, float(value))

    @property
    def max_length(self) -> float:
        return distance_joint_module.get_max_length(self._component_id)

    @max_length.setter
    def max_length(self, value: float):
        distance_joint_module.set_max_length(self._component_id, float(value))

    @property
    def enable_motor(self) -> bool:
        return distance_joint_module.get_enable_motor(self._component_id)

    @enable_motor.setter
    def enable_motor(self, value: bool):
        distance_joint_module.set_enable_motor(self._component_id, bool(value))

    @property
    def motor_speed(self) -> float:
        return distance_joint_module.get_motor_speed(self._component_id)

    @motor_speed.setter
    def motor_speed(self, value: float):
        distance_joint_module.set_motor_speed(self._component_id, float(value))

    @property
    def max_motor_force(self) -> float:
        return distance_joint_module.get_max_motor_force(self._component_id)

    @max_motor_force.setter
    def max_motor_force(self, value: float):
        distance_joint_module.set_max_motor_force(self._component_id, float(value))

    @property
    def current_length(self) -> float:
        """Live separation between the anchors, in pixels. Read-only."""
        return distance_joint_module.get_current_length(self._component_id)
