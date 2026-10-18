from Domain import *


class NewScript(ScriptableComponent):
    """A new RockEngine script. Annotated class attributes become
    inspector fields, e.g. `speed: float = 5.0`.
    """

    rot_speed : float = 1

    def start(self):
        pass

    def update(self):
        self.transform.rotation += Time.delta_time * self.rot_speed
        pass
