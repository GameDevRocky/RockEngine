import random

from Domain import *


class TopDownController(ScriptableComponent):
    velocity : Reflect[Vector2, ReadOnly()]
    test : Reflect[Vector3, Range(0, 1)]
    comboBox : Reflect[int, Options("Idle", "Run", "Jump")]
    slider : list[Reflect[Vector3, Range(0, 1)]]

    def awake(self):
        pass

    def update(self):
        self.transform.rotation = Vector2.angle_to(self.transform.position, Input.get_mouse_pos())
        
    pass