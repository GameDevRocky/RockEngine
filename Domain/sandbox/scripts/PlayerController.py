from Domain import *



class PlayerController(ScriptableComponent):
    camera : Camera


    def fixed_update(self):
        self.camera.transform.position += (self.transform.position - self.camera.transform.position) * 0.05


