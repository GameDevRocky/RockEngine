from Domain import *

class BallScript(ScriptableComponent):
    new_var : float = 0.25
    def awake(self):
        self.sr = self.get_component(SpriteRenderer)
        self.sprites = [Sprite(f"idle_{i}") for i in range(11)]

    def start(self):
        self.start_coroutine(self.animate())

    def animate(self):
        frame = 0
        while True:
            self.sr.sprite = self.sprites[frame]
            frame = (frame + 1) % len(self.sprites)
            yield WaitForSeconds(self.new_var/10)
