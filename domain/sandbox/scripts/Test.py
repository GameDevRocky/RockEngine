from Domain import *

class TestScript(ScriptableComponent):

    def awake(self):
        self.velocity = Vector2(0,0)
        self.rb = self.get_component(Rigidbody)
            
    def fixed_update(self):
        self.transform.world_position = Input.get_mouse_pos()
