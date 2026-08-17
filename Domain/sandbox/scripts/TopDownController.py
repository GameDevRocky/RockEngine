import random

from Domain import *

from Raycaster import Raycaster

class TopDownController(ScriptableComponent):

    velocity : Reflect[Vector2, ReadOnly()] = Vector2(0,0)
    speed : Reflect[float, Slider, Step(1), Range(1, 100)] = 1.00
    friction : Reflect[float, Slider, Step(0.05), Range(0, 1)] = 0.8
    camera : Camera

    light : Light = None

    camera_smoothing : Reflect[float, Slider, Step(0.1), Range(1, 10)] = 0.1

    rb : Reflect[Rigidbody, ReadOnly()]
    bc : Reflect[BoxCollider, ReadOnly()]
    muzzle : ParticleComponent


    def awake(self):
        self.rb = self.get_component(Rigidbody)
        self.rb.body_type = Rigidbody.DYNAMIC

        self.bc = self.get_component(BoxCollider)

    def update(self):
        if self.light:
            if Input.mouse_pressed(MouseButton.MIDDLE):
                self.light.enabled = not self.light.enabled

        if Input.mouse_down(MouseButton.LEFT):
            if self.muzzle:
                self.muzzle.emit_burst(1)
          

    def fixed_update(self): 
        if self.camera:
            self.camera.transform.position += (self.transform.position - self.camera.transform.position) * self.camera_smoothing * 0.01

        mouse_pos = Input.get_mouse_pos()

        left Input.is_key_down(Keys.D) or Gamepad.axis(Gamepad.)


        input_ = Vector2(
            Input.is_key_down(Keys.D) - Input.is_key_down(Keys.A),
            Input.is_key_down(Keys.W) - Input.is_key_down(Keys.S)
        )

        direction = mouse_pos - self.transform.position
        self.rb.apply_impulse(input_.normalize() * self.speed * 100)
        self.rb.velocity *= self.friction
        self.transform.rotation = math.degrees(math.atan2(direction.y, direction.x))
        self.velocity = self.rb.velocity
        Debug.draw_circle(mouse_pos, 40)


    @action
    def Test_Function(self, val : str):
        Console.comment(f'{val} : {Time.elapsed_time}')
 