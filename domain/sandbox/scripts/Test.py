from Domain import *

class TestScript(ScriptableComponent):

    def awake(self):
        self.velocity = Vector2(0,0)
        self.renderer = self.get_component(SpriteRenderer)
            
    def fixed_update(self):
        input = Vector2(int(Input.is_key_down(Keys.D) - Input.is_key_down(Keys.A)), int(Input.is_key_down(Keys.W) - Input.is_key_down(Keys.S)))
        if Input.is_key_down(Keys.D):
            self.transform.scale = [1 , 1]
        if Input.is_key_down(Keys.A):
            self.transform.scale = [-1 , 1]
        self.velocity += input * .5
        self.transform.position += self.velocity
        self.velocity *= 0.8

        

    
    