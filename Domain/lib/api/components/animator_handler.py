from rock_engine.components import animator_module
from .component_handler import Component


class Animator(Component):
    """Scripting handle for the Animator state machine. Set parameters (which
    drive transitions) and query/force the current state."""
    _type_name = "Animator"

    def __init__(self, obj_id=None):
        super().__init__(obj_id)

    def set_float(self, name: str, value: float):
        animator_module.set_float(self._gameobject_id, str(name), float(value))

    def set_int(self, name: str, value: int):
        animator_module.set_int(self._gameobject_id, str(name), int(value))

    def set_bool(self, name: str, value: bool):
        animator_module.set_bool(self._gameobject_id, str(name), bool(value))

    def set_trigger(self, name: str):
        animator_module.set_trigger(self._gameobject_id, str(name))

    def reset_trigger(self, name: str):
        animator_module.reset_trigger(self._gameobject_id, str(name))

    def get_float(self, name: str) -> float:
        return animator_module.get_float(self._gameobject_id, str(name))

    def get_int(self, name: str) -> int:
        return animator_module.get_int(self._gameobject_id, str(name))

    def get_bool(self, name: str) -> bool:
        return animator_module.get_bool(self._gameobject_id, str(name))

    @property
    def current_state(self) -> str:
        return animator_module.get_current_state(self._gameobject_id)

    def play(self, state_name: str):
        """Force-enter a state immediately (ignores transitions)."""
        animator_module.play(self._gameobject_id, str(state_name))
