from Domain import *
import random
import math

class BallScript(ScriptableComponent):


    def awake(self):
        self.rb = self.get_component(Rigidbody)
        self.collider = self.get_component(CircleCollider)
        self.collider.bounciness = 1.00
        self.gameobject.tag = "Ball"
    
    def start(self):
        self.rb.apply_impulse((-150, 0))
    
    def on_collision_enter(self, other):
        if other.gameobject.tag != "Paddle":
            return

        vel = self.rb.velocity
        speed = math.sqrt(vel.x * vel.x + vel.y * vel.y)
        speed = max(10.0, min(speed, 150.0))

        dir = (self.transform.position - other.transform.position).normalize()

        # Clamp angle to at most 60 degrees from horizontal so the ball never
        # travels nearly straight up or down after a paddle hit.
        MAX_ANGLE = math.radians(60)
        angle = math.atan2(dir.y, dir.x)
        if abs(angle) > MAX_ANGLE:
            angle = math.copysign(MAX_ANGLE, angle)
            dir = Vector2(math.cos(angle), math.sin(angle))

        self.rb.velocity = Vector2(0, 0)
        self.rb.apply_impulse(dir * speed)

    