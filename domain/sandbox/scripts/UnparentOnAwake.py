from Domain import *


class UnparentOnAwake(ScriptableComponent):

    def awake(self):
        self.parent = self.transform.parent
        self._space_was_down = False

    def update(self):
        space_down = Input.is_key_down(Keys.SPACE)
        if space_down and not self._space_was_down:
            if not self.transform.parent:
                self.transform.parent = self.parent
            else:
                self.transform.parent = None
        self._space_was_down = space_down

