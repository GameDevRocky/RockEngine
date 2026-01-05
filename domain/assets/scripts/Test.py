from Domain import *

class TestScript(ScriptableComponent):
        
    def update(self):
        self.transform.rotation -= 1
    
    