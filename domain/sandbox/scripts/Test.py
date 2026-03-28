from Domain import *

class TestScript(ScriptableComponent):
    def awake(self):
        Console.comment(self.gameobject.name)
    def fixed_update(self):
        self.transform.position = Input.get_mouse_pos()

