from Domain import *
import math


class CameraShake(ScriptableComponent):
    """Applies a temporary, decaying positional shake to this camera."""

    strength : Reflect[float, Slider, Step(0.5), Range(0, 100),
                       Tooltip("Maximum camera displacement in world units.")] = 18.0
    duration : Reflect[float, Slider, Step(0.01), Range(0, 2),
                       Tooltip("How long each shake lasts in seconds.")] = 0.18
    frequency : Reflect[float, Slider, Step(1), Range(1, 60),
                        Tooltip("How quickly the shake changes direction.")] = 28.0
    decay : Reflect[bool,
                    Tooltip("Fade the shake strength to zero over its duration.")] = True
    dx : Reflect[float, ReadOnly()] = 0.0
    dy : Reflect[float, ReadOnly()] = 0.0

    def awake(self):
        self._remaining = 0.0
        self._phase = 0.0
        self._pending_offset = Vector2(0, 0)

    def update(self):
        # The offset is temporary. Restore the unshaken position before other
        # variable-rate camera work observes it, then apply the next sample in
        # late_update so rendering sees the shake.
        self._remove_applied_offset()
        self._pending_offset = Vector2(0, 0)

        if self._remaining <= 0.0 or self.duration <= 0.0 or self.strength <= 0.0:
            self._remaining = 0.0
            return

        dt = max(0.0, Time.delta_time)
        self._remaining = max(0.0, self._remaining - dt)
        self._phase += max(1.0, self.frequency) * math.tau * dt

        envelope = self._remaining / self.duration if self.decay else 1.0
        amplitude = self.strength * envelope

        # Two differently phased waves avoid the harsh, frame-rate-dependent
        # flicker of choosing a new random offset every frame.
        x = math.sin(self._phase) + 0.5 * math.sin(self._phase * 2.17 + 1.31)
        y = math.sin(self._phase * 1.37 + 2.09) + 0.5 * math.sin(self._phase * 2.73)
        self._pending_offset = Vector2(x, y) * (amplitude / 1.5)

    def late_update(self):
        self.dx = self._pending_offset.x
        self.dy = self._pending_offset.y
        self.transform.position += self._pending_offset

    def on_shutdown(self):
        # Do not leave the edit-time camera displaced when play mode ends.
        self._remove_applied_offset()

    @action("Shake", tooltip="Start or refresh this camera's configured shake.")
    def shake(self):
        self._remaining = max(0.0, self.duration)
        # Advance the phase so repeated shots do not begin on the same sample.
        self._phase += 2.399963229728653

    def _remove_applied_offset(self):
        if self.dx != 0.0 or self.dy != 0.0:
            self.transform.position -= Vector2(self.dx, self.dy)
            self.dx = 0.0
            self.dy = 0.0
