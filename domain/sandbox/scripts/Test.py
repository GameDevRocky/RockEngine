from Domain import *

class TestScript(ScriptableComponent):
    def awake(self):
        self.tick = 0

    def fixed_update(self):
        self.transform.position = Input.get_mouse_pos()
        self.gameobject.name = self.tick 
        self.tick += 1

