from Domain import *


class CameraFollow(ScriptableComponent):
    """A new RockEngine script. Annotated class attributes become
    inspector fields, e.g. `speed: float = 5.0`.
    """

    focus : GameObject = None


    def start(self):
        pass

    def fixed_update(self):
        if not self.focus:
            return
        self.transform.position.y += (self.focus.transform.position.y - self.transform.position.y) * 0.05
        