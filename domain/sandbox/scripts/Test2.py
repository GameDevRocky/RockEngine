from Domain import *
import math

class TestScript2(ScriptableComponent):

    def awake(self):
        self.start_pos = self.transform.world_position
        self.counter = 0.0
        self.amplitude = 50.0
        self.speed = 0.005

    def update(self):
        self.counter += 1.0
        self.transform.rotation += 1
        offset_x = math.sin(self.counter * self.speed) * self.amplitude
        if Input.is_key_down(Keys.SPACE):
            for i in range(1000):
                print(i)
        
        new_pos = (self.start_pos.x + offset_x, self.start_pos.y)
        self.transform.world_position = new_pos