from rock_engine.systems import time_module


class _Time:
    @property
    def delta_time(self) -> float:
        return time_module.get_delta_time()

    @property
    def fixed_delta_time(self) -> float:
        return time_module.get_fixed_delta_time()

    @property
    def elapsed_time(self) -> float:
        return time_module.get_elapsed_time()

    @property
    def fps(self) -> float:
        return time_module.get_fps()

    @property
    def time_scale(self) -> float:
        return time_module.get_time_scale()

    @time_scale.setter
    def time_scale(self, value: float):
        time_module.set_time_scale(float(value))


Time = _Time()
