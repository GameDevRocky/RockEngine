from Domain import *

class TestScript(ScriptableComponent):

    def awake(self):
        self.velocity = Vector2(0,0)
        self.renderer = self.get_component(SpriteRenderer)
        self.renderer.color = [1,1,1,0.5]
            
    def update(self):
        Console.comment(self.renderer.color)
        input = Vector2(int(Input.is_key_down(Keys.D) - Input.is_key_down(Keys.A)), int(Input.is_key_down(Keys.W) - Input.is_key_down(Keys.S)))
        if Input.is_key_down(Keys.D):
            self.renderer.flipX = False
        if Input.is_key_down(Keys.A):
            self.renderer.flipX = True
        self.velocity += input * .5
        self.transform.position += self.velocity
        self.velocity *= 0.8

        

    
    