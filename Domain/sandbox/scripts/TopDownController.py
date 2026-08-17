import random

from Domain import *


class TopDownController(ScriptableComponent):
    velocity : Reflect[Vector2, ReadOnly()]
    test : Reflect[Vector3, Range(0, 1)]
    comboBox : Reflect[int, Options("Idle", "Run", "Jump")]
    sliders : list[Reflect[float, Range(0, 100), Slider]]
    event : Event = None

    def awake(self):
        self.event.invoke()
        
        pass

    def update(self):

        if Input.is_key_pressed(Keys.SPACE):
            self.event.invoke()

        self.transform.rotation = Vector2.angle_to(self.transform.position, Input.get_mouse_pos())


    @action
    def Test_Function(self, val : str):
        Console.comment(f'{val} : {Time.elapsed_time}')
 