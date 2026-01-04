
class Script:
    def __init__(self):
        self._gameobject_id = None  # injected by C++

    @property
    def gameobject(self):
        return None

    @property
    def transform(self):
        return None

    def delta_time(self):
        return None

    def awake(self): pass
    def start(self): pass
    def update(self): pass
    def fixed_update(self): pass
    def late_update(self): pass
    def on_destroy(self): pass
