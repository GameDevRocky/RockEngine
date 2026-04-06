from Domain import *

class TestScript(ScriptableComponent):
       
    def awake(self):
        self.tick = 0

    def start(self):
        self.sprite_renderer = self.get_component(SpriteRenderer)

    def late_update(self):
        self.transform.position = Input.get_mouse_pos()

    def on_trigger_enter(self, other : Collider):
        other.get_component(SpriteRenderer).color = (0, 1, 0, 1)
        self.sprite_renderer.color = (1, 0, 0, 1)

    def on_trigger_exit(self, other : Collider):
        other.get_component(SpriteRenderer).color = (1, 1, 1, 1)
        self.sprite_renderer.color = (1, 1, 1, 1)


