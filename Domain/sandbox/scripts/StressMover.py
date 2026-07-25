from Domain import *


class StressMover(ScriptableComponent):
    """Drives a body left/right on a timer. Used by stress200.scene to keep a
    handful of bodies permanently awake and exercising the script boundary."""

    speed: float = 150.0
    period: float = 2.0

    def awake(self):
        self.rb = self.get_component(Rigidbody)
        self.t = 0.0

    def fixed_update(self):
        if not self.rb:
            return
        self.t += Time.fixed_delta_time
        vel = self.rb.velocity
        vel.x = self.speed if (self.t % (2 * self.period)) < self.period else -self.speed
        self.rb.velocity = vel
