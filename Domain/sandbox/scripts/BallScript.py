from Domain import *
from Test import TestScript
class BallScript(ScriptableComponent):
    new_var : float = 0.5
    sprites : list[Sprite]
    obj : list[GameObject]
    test:str

    def awake(self):
        self.sr = self.get_component(SpriteRenderer)
        self.sprites.extend([Sprite(f"idle_{i}") for i in range(11)])
        self.rb = self.get_component(Rigidbody)
        self.rb.lock_rotation = True
        self.rb.use_gravity = True
 

    def start(self):
        self.start_coroutine(self.animate())

    def update(self):
        vel = Input.is_key_down(Keys.D) - Input.is_key_down(Keys.A)
        vel = (vel * 25, Input.is_key_pressed(Keys.W ) * 250 )
        self.rb.apply_impulse(vel)
        pass

    def animate(self):
        frame = 0
        while True:
            self.sr.sprite = self.sprites[frame]
            frame = (frame + 1) % len(self.sprites)
            yield WaitForSeconds(self.new_var/10)
