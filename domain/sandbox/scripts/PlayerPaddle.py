from Domain import *
import random
import math

class PlayerPaddle(ScriptableComponent):


    def update(self):
        pos = Input.get_mouse_pos()
        self.transform.position = (self.transform.position.x, pos.y)