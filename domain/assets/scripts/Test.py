from Domain.lib.engine import Script

class TestScript(Script):
    def awake(self):
        print("Hello From Test script")
        

    def update(self):
        print("Updating from test script")
    
    def fixed_update(self):
        return super().fixed_update()

    def late_update(self):
        return super().late_update()