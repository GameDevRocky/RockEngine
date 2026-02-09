from Domain import *

class TestScript(ScriptableComponent):

    def awake(self):
        self.velocity = Vector2(0,0)
        self.rb = self.get_component(Rigidbody)
            
    def fixed_update(self):
        self.transform.position = Input.get_mouse_pos()
        if (Input.is_key_down(Keys.D)):
            self.rb.apply_force([100, 0])
        elif (Input.is_key_down(Keys.A)):
            self.rb.apply_force([-100, 0])
        elif (Input.is_key_down(Keys.W)):
            self.rb.apply_impulse([0, 100])

        

    
    