class WaitForSeconds:
    """Suspend the coroutine for `seconds` of scaled game time."""
    def __init__(self, seconds: float):
        self._remaining = float(seconds)

    def tick(self, dt: float) -> bool:
        self._remaining -= dt
        return self._remaining <= 0.0


class WaitForFrames:
    """Suspend the coroutine for `frames` update frames."""
    def __init__(self, frames: int):
        self._remaining = int(frames)

    def tick(self, dt: float) -> bool:
        self._remaining -= 1
        return self._remaining <= 0


class WaitUntil:
    """Suspend the coroutine until `condition()` returns truthy."""
    def __init__(self, condition : function):
        self._condition = condition

    def tick(self, dt: float) -> bool:
        return bool(self._condition())


class WaitForEndOfFrame:
    """Suspend the coroutine until the end of the current frame."""
    def tick(self, dt: float) -> bool:
        return True
