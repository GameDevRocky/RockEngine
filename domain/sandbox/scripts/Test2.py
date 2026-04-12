from Domain import *
import math

class TestScript2(ScriptableComponent):

    def awake(self):
        self.start_pos = self.transform.world_position
        self.counter = 0.0
        self.amplitude = 200.0
        self.speed = 0.05
        self.sr = self.get_component(SpriteRenderer)

    def fixed_update(self):
        self.counter += 1.0
        self.transform.rotation += 1
        offset_x = math.sin(self.counter * self.speed) * self.amplitude
        new_pos = (self.start_pos.x + offset_x, self.start_pos.y)
        self.transform.world_position = new_pos
        self.sr.color = (math.sin(self.counter), math.cos(self.counter), math.tan(self.counter), 1)