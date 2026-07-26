import random

from Domain import *


class FireworksSpawner(ScriptableComponent):
    """Left-click anywhere in the game view to launch a fireworks burst at the
    mouse position.

    ── Editor setup ──────────────────────────────────────────────────────────
    Attach to any single GameObject in the scene (e.g. an empty "Fireworks"
    manager object) -- it doesn't render anything itself, it just listens for
    clicks and spawns one-shot particle bursts.
    """

    particle_count: int = 80              # particles per burst
    lifetime: Vector2 = Vector2(0.6, 1.3)  # min,max seconds a particle lives
    speed: Vector2 = Vector2(150.0, 450.0) # min,max initial particle speed
    start_size: float = 12.0
    end_size: float = 0.0
    gravity: float = -260.0                # slight downward pull as the burst falls
    damping: float = 0.6                   # slows particles down over their life
    sprite: Sprite = None                  # optional: texture each particle uses (blank = untextured quad)

    # One of these is picked at random for each burst.
    _palette = [
        Vector4(1.0, 0.25, 0.2, 1.0),   # red
        Vector4(0.25, 0.6, 1.0, 1.0),   # blue
        Vector4(0.3, 1.0, 0.4, 1.0),    # green
        Vector4(1.0, 0.85, 0.2, 1.0),   # gold
        Vector4(0.85, 0.3, 1.0, 1.0),   # purple
        Vector4(1.0, 1.0, 1.0, 1.0),    # white
    ]

    def update(self):
        # Edge-triggered: one burst per click, not one per frame held down.
        if Input.mouse_pressed(MouseButton.LEFT):
            self._launch(Input.get_mouse_pos())

    def _launch(self, pos: Vector2):
        obj = self.instantiate("Firework")
        obj.transform.position = pos

        p = obj.add_component(ParticleComponent)
        p.shape = ParticleComponent.Shape.POINT
        p.space = ParticleComponent.Space.WORLD
        p.blend_mode = ParticleComponent.Blend.ADDITIVE
        p.looping = False
        p.emission_rate = 0.0            # no continuous stream -- burst only
        p.max_particles = self.particle_count
        p.duration = self.lifetime.y      # keep the emitter alive as long as any particle can live

        if self.sprite:
            p.sprite = self.sprite

        p.direction = 90.0
        p.spread = 360.0                  # radial explosion in every direction
        p.speed = self.speed
        p.lifetime = self.lifetime
        p.gravity = Vector2(0.0, self.gravity)
        p.damping = self.damping

        color = random.choice(self._palette)
        p.start_color = color
        p.end_color = Vector4(color.x, color.y, color.z, 0.0)   # fade to transparent
        p.start_size = self.start_size
        p.end_size = self.end_size

        p.emit_burst(self.particle_count)

        self.start_coroutine(self._cleanup(obj))

    def _cleanup(self, obj):
        # Give the last particle time to finish fading before tearing down.
        yield WaitForSeconds(self.lifetime.y + 0.25)
        obj.destroy()
