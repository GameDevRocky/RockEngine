from Domain import *

class TestScript(ScriptableComponent):

    def awake(self):
        self.gameobject.name = "test"
        Console.alert(self.gameobject.active)

    def fixed_update(self):
        self.transform.position = Input.get_mouse_pos()

