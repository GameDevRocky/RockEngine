import engine_api

class ScriptableComponent:
    def __init__(self):
        # This ID is injected by your C++ InstantiateScript()
        self._gameobject_id = None 

    @property
    def transform(self):
        # We create a HANDLE on the fly using the ID
        return TransformHandle(self._gameobject_id)


class TransformHandle:
    def __init__(self, obj_id):
        self.id = obj_id

    @property
    def position(self):
        return engine_api.get_position(self.id)

    @position.setter
    def position(self, value):
        engine_api.set_position(self.id, float(value[0]), float(value[1]))

    @property
    def rotation(self):
        return engine_api.get_rotation(self.id)

    @rotation.setter
    def rotation(self, value):
        engine_api.set_rotation(self.id, float(value))

    @property
    def scale(self):
        return engine_api.get_scale(self.id)

    @scale.setter
    def scale(self, value):
        engine_api.set_scale(self.id, float(value[0]), float(value[1]))