from Domain import *


class ThwompController(ScriptableComponent):
    """Drives a block between the limits of a vertical PrismaticJoint."""

    # Component references serialize through their owning GameObject. Resolve the
    # exact repeatable joint instance at runtime so its component id is populated.
    joint: GameObject = None

    slam_speed: Reflect[float, Slider, Step(25), Range(25, 2000),
                        Tooltip("Downward speed in pixels per second.")] = 650.0
    rise_speed: Reflect[float, Slider, Step(10), Range(10, 1000),
                        Tooltip("Upward return speed in pixels per second.")] = 140.0
    motor_force: Reflect[float, Slider, Step(1000), Range(1000, 500000),
                         Tooltip("Maximum force available to move the block.")] = 100000.0
    bottom_pause: Reflect[float, Slider, Step(0.05), Range(0, 5)] = 0.45
    top_pause: Reflect[float, Slider, Step(0.05), Range(0, 5)] = 1.0
    arrival_tolerance: Reflect[float, Slider, Step(0.5), Range(0.5, 20)] = 2.0

    state: Reflect[str, ReadOnly()] = "Disabled"

    def start(self):
        self._timer = 0.0
        self._joint = (self.joint.get_component(PrismaticJoint)
                       if self.joint else None)

        if not self._joint:
            self.state = "Disabled"
            Console.warn(f"{self.gameobject.name}: no PrismaticJoint assigned.")
            return

        self._joint.enable_limit = True
        self._joint.enable_motor = True
        self._joint.max_motor_force = self.motor_force
        self._set_state("Top Pause")

    def fixed_update(self):
        if not self._joint:
            return

        dt = Time.fixed_delta_time
        translation = self._joint.translation

        if self.state == "Top Pause":
            self._timer -= dt
            if self._timer <= 0.0:
                self._set_state("Slamming")
        elif self.state == "Slamming":
            if translation <= self._joint.lower_translation + self.arrival_tolerance:
                self._set_state("Bottom Pause")
        elif self.state == "Bottom Pause":
            self._timer -= dt
            if self._timer <= 0.0:
                self._set_state("Rising")
        elif self.state == "Rising":
            if translation >= self._joint.upper_translation - self.arrival_tolerance:
                self._set_state("Top Pause")

    def _set_state(self, new_state):
        self.state = new_state

        if new_state == "Top Pause":
            self._joint.motor_speed = 0.0
            self._timer = self.top_pause
        elif new_state == "Slamming":
            self._joint.max_motor_force = self.motor_force
            self._joint.motor_speed = -self.slam_speed
        elif new_state == "Bottom Pause":
            self._joint.motor_speed = 0.0
            self._timer = self.bottom_pause
        elif new_state == "Rising":
            self._joint.max_motor_force = self.motor_force
            self._joint.motor_speed = self.rise_speed
