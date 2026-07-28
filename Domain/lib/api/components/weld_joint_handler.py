from rock_engine.components import weld_joint_module
from .joint_handler import Joint


class WeldJoint(Joint):
    """Rigidly fuses two bodies.

    A hertz of 0 means maximum stiffness; raising it makes the weld springy,
    which is how you fake soft-body wobble.
    """

    _type_name = "WeldJoint"

    @property
    def linear_hertz(self) -> float:
        return weld_joint_module.get_linear_hertz(self._component_id)

    @linear_hertz.setter
    def linear_hertz(self, value: float):
        weld_joint_module.set_linear_hertz(self._component_id, float(value))

    @property
    def linear_damping_ratio(self) -> float:
        return weld_joint_module.get_linear_damping_ratio(self._component_id)

    @linear_damping_ratio.setter
    def linear_damping_ratio(self, value: float):
        weld_joint_module.set_linear_damping_ratio(self._component_id, float(value))

    @property
    def angular_hertz(self) -> float:
        return weld_joint_module.get_angular_hertz(self._component_id)

    @angular_hertz.setter
    def angular_hertz(self, value: float):
        weld_joint_module.set_angular_hertz(self._component_id, float(value))

    @property
    def angular_damping_ratio(self) -> float:
        return weld_joint_module.get_angular_damping_ratio(self._component_id)

    @angular_damping_ratio.setter
    def angular_damping_ratio(self, value: float):
        weld_joint_module.set_angular_damping_ratio(self._component_id, float(value))
