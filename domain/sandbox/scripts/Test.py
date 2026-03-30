from Domain import *

class TestScript(ScriptableComponent):

    def fixed_update(self):
        self.transform.position = Input.get_mouse_pos()
        Console.alert("BITCH")
