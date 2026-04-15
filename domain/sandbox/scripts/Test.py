from Domain import *
from typing import Annotated as Reflect

class TestScript(ScriptableComponent):
    speed: float = 500
    jump_force: float = 300

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
        result = Physics.cast_ray(self.transform.position + (0, 0), Vector2(0, -64))
        if result:
            self.rb.velocity = (self.rb.velocity.x, 0)
            self.grounded = True
        else:
            self.grounded = False
        if self.grounded and Input.is_key_down(Keys.W):
            self.rb.apply_impulse((0, self.jump_force))

