from Domain import *


class EnemyFlash(ScriptableComponent):
    flash_material: Material = None

    _flash_timer = 0.0
    _flash_duration = 0.1
    _sprite_renderers = []

    def awake(self):
        # Script fields declared at class scope are shared defaults. Give each
        # component its own renderer collection before walking the hierarchy.
        self._sprite_renderers = []
        self._collect_sprite_renderers(self.transform)

    def update(self):
        if self._flash_timer <= 0.0:
            return

        self._flash_timer -= Time.delta_time
        if self._flash_timer <= 0.0:
            self._set_flash_amount(0.0)
            return

        self._set_flash_amount(self._flash_timer / self._flash_duration)

    def _collect_sprite_renderers(self, transform):
        """Collect the root renderer and every renderer below it."""
        for sprite_renderer in transform.gameobject.get_components(SpriteRenderer):
            self._sprite_renderers.append(sprite_renderer)
            if self.flash_material:
                sprite_renderer.material = self.flash_material
                sprite_renderer.set_uniform("uFlashAmount", 0.0)

        for child in transform.children:
            self._collect_sprite_renderers(child)

    def _set_flash_amount(self, amount):
        """Flash every renderer without mutating the shared material asset."""
        for sprite_renderer in self._sprite_renderers:
            sprite_renderer.set_uniform("uFlashAmount", amount)

    @action
    def trigger_flash(self):
        self._flash_timer = self._flash_duration
        self._set_flash_amount(2.0)

    def on_collision_enter(self, other):
        if other.gameobject.tag == "Bullet":
            self.trigger_flash()
