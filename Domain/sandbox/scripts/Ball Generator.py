from Domain import *
import random
import math

class BallGenerator(ScriptableComponent):
    power : int = 30
    sprite : Sprite = None
    scale : float = 10
    

    def update(self):
        if Input.mouse_down(MouseButton.LEFT):
            pos = Input.get_mouse_pos()
            obj = self.instantiate("obj")
            sr = obj.add_component(SpriteRenderer)
            sr.sprite = self.sprite
            cc = obj.add_component(BoxCollider)
            rb = obj.get_component(Rigidbody)
            rb.body_type = Rigidbody.DYNAMIC
            obj.transform.position = pos
            obj.transform.scale = self.scale
            sr.color = (random.random(), random.random(), random.random(), 1.0)




    