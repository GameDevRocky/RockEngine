from Domain import *

class TestScript(ScriptableComponent):
       
    def awake(self):
        self.tick = 0
        self.grounded = False

    def start(self):
        self.rb = self.get_component(Rigidbody)
        self.rb.enabled = False
        self.sprite_renderer = self.get_component(SpriteRenderer)

    def fixed_update(self):

        if Input.is_key_down(Keys.A):
            self.rb.apply_force((-1000, 0))
            self.sprite_renderer.flipX = True
        if Input.is_key_down(Keys.D):
            self.rb.apply_force((1000, 0))
            self.sprite_renderer.flipX = False
        if self.grounded and Input.is_key_down(Keys.W):
            self.rb.apply_impulse((0, 200))
        

    def on_collision_enter(self, other : Collider):
        self.grounded = True

    def on_collision_exit(self, other : Collider):
        self.grounded = False


