from Domain import *


class Countdown(ScriptableComponent):
    beat_duration: Reflect[float, Slider, Step(0.1), Range(0.5, 5.0)] = 2.0
    pop_duration: Reflect[float, Slider, Step(0.05), Range(0.05, 1.0)] = 0.25
    fade_duration: Reflect[float, Slider, Step(0.05), Range(0.05, 1.5)] = 0.6
    pop_scale: Reflect[float, Slider, Step(0.05), Range(1.0, 3.0)] = 1.45
    end_scale: Reflect[float, Slider, Step(0.05), Range(0.25, 1.0)] = 0.75

    def start(self):
        self.text_renderer = self.get_component(TextRenderer)
        if not self.text_renderer:
            Console.warn("Countdown requires a TextRenderer.")
            return

        self._base_scale = Vector2(self.transform.scale)
        self._base_color = list(self.text_renderer.color)
        self.text_renderer.visible = True
        self.start_coroutine(self.countdown())

    def countdown(self):
        for label in ("3", "2", "1", "GO!"):
            yield from self._animate_beat(label)

        self.text_renderer.text = ""
        self.text_renderer.visible = False
        self.transform.scale = self._base_scale
        self.text_renderer.color = self._base_color

    def _animate_beat(self, label):
        self.text_renderer.text = label
        self.text_renderer.color = self._color_with_alpha(1.0)
        self.transform.scale = self._base_scale * self.pop_scale

        # Pop in oversized, then settle quickly with a cubic ease-out.
        elapsed = 0.0
        while elapsed < self.pop_duration:
            elapsed += Time.delta_time
            t = min(1.0, elapsed / self.pop_duration)
            eased = 1.0 - (1.0 - t) ** 3
            scale = self.pop_scale + (1.0 - self.pop_scale) * eased
            self.transform.scale = self._base_scale * scale
            yield None

        # Keep the number readable before its exit animation begins.
        hold_duration = max(0.0, self.beat_duration
                            - self.pop_duration - self.fade_duration)
        if hold_duration > 0.0:
            yield WaitForSeconds(hold_duration)

        # Fade and shrink together so the next beat has a clean visual reset.
        elapsed = 0.0
        while elapsed < self.fade_duration:
            elapsed += Time.delta_time
            t = min(1.0, elapsed / self.fade_duration)
            self.text_renderer.color = self._color_with_alpha(1.0 - t)
            scale = 1.0 + (self.end_scale - 1.0) * t
            self.transform.scale = self._base_scale * scale
            yield None

    def _color_with_alpha(self, alpha):
        return [self._base_color[0], self._base_color[1],
                self._base_color[2], self._base_color[3] * alpha]
