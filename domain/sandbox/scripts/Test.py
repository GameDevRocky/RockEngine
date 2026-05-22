from Domain import *
from typing import Annotated as Reflect

class TestScript(ScriptableComponent):
    speed: float = 500
    jump_force: float = 100
    scale : Reflect[float, Step(1)] = 32
    testVar : float = 0.1

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
        if Input.is_key_down(Keys.SPACE):
            pos = Input.get_mouse_pos()
            new_obj = self.instantiate("GameObject")
            new_obj.transform.position = pos
            sr = new_obj.add_component(SpriteRenderer)
            sr.sprite = Sprite("sprite3")
            rb = new_obj.add_component(Rigidbody)
            cc = new_obj.add_component(CircleCollider)
            cc.friction = 0.5
            cc.bounciness = 1.0
            rb.apply_impulse([random.randint(-500, 500), 500])
            

            
        

    def update(self):
        result = Physics.cast_ray(self.transform.position, -self.transform.up * self.scale)
        if result:
            self.grounded = True
        else:
            self.grounded = False
        if self.grounded and Input.is_key_down(Keys.W):
            self.rb.apply_impulse((0, self.jump_force))