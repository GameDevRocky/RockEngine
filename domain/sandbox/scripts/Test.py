from Domain import *
from typing import Annotated as Reflect

class TestScript(ScriptableComponent):
    speed: float = 500
    jump_force: float = 100
    scale : Reflect[float, Step(1)] = 32

    def init(self):
        pass

    def awake(self):
        self.grounded = False
    def start(self):
        self.rb = self.get_component(Rigidbody)
        self.rb.enabled = False
        self.sprite_renderer = self.get_component(SpriteRenderer)

    def fixed_update(self):
        self.pos = self.transform.position
        if Input.is_key_down(Keys.A):
            self.rb.apply_force((-self.speed, 0))
            self.sprite_renderer.flipX = True
        if Input.is_key_down(Keys.D):
            self.rb.apply_force((self.speed, 0))
            self.sprite_renderer.flipX = False
        

    def update(self):
        result = Physics.cast_ray(self.transform.position, -self.transform.up * self.scale)
        if result:
            self.grounded = True
        else:
            self.grounded = False
        if self.grounded and Input.is_key_down(Keys.W):
            self.rb.apply_impulse((0, self.jump_force))