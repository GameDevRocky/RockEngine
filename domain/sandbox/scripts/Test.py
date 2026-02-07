from Domain import *

class TestScript(ScriptableComponent):

    def awake(self):
        self.enemy = GameObject("o2")
        self.velocity = Vector2(0,0)
            
    def fixed_update(self):
        self.transform.position = Input.get_mouse_pos()

        

    
    