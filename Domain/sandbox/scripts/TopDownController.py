import random

from Domain import *

from Raycaster import Raycaster

class TopDownController(ScriptableComponent):


    velocity : Reflect[Vector2, ReadOnly()] = Vector2(0,0)
    speed : Reflect[float, Slider, Step(1), Range(1, 100)] = 1.00
    friction : Reflect[float, Slider, Step(0.05), Range(0, 1)] = 0.8
    camera : Camera

    light : Light = None

    camera_smoothing : Reflect[float, Slider, Step(0.1), Range(1, 10)] = 0.1

    rb : Reflect[Rigidbody, ReadOnly()]
    bc : Reflect[BoxCollider, ReadOnly()]
    muzzle : ParticleComponent
    bullet_trail : ParticleComponent = None
    bullet_impact : ParticleComponent = None

    # ── Weapon ──────────────────────────────────────────────────────────────
    fire_range : Reflect[float, Slider, Step(50), Range(100, 5000)] = 2000.0
    fire_rate : Reflect[float, Slider, Step(0.02), Range(0.02, 1.0),
                        Tooltip("Seconds between shots while the button is held.")] = 0.12
    tracer_steps : Reflect[int, Slider, Step(1), Range(1, 30),
                           Tooltip("Frames the tracer takes to travel. 1 = instant hitscan streak.")] = 6
    tracer_density : Reflect[int, Slider, Step(1), Range(1, 20)] = 3
    impact_count : Reflect[int, Slider, Step(1), Range(1, 64)] = 14
    impact_spread : Reflect[float, Slider, Step(5), Range(0, 180),
                            Tooltip("Cone width of the impact spray around the bounce direction.")] = 40.0
    muzzle_count : Reflect[int, Slider, Step(1), Range(1, 32)] = 4


    def awake(self):
        self.rb = self.get_component(Rigidbody)
        self.rb.body_type = Rigidbody.DYNAMIC

        self.bc = self.get_component(BoxCollider)

        self._next_shot = 0.0

        # Both effect emitters get repositioned in world space and left there, so they
        # MUST simulate in world space. In LOCAL space every particle already alive is
        # parented to the emitter, so moving it to the next tracer step would drag the
        # whole trail along behind the bullet instead of leaving it in the air.
        for emitter in (self.bullet_trail, self.bullet_impact):
            if emitter:
                emitter.space = ParticleComponent.Space.WORLD

    def update(self):
        if self.light:
            if Input.mouse_pressed(MouseButton.MIDDLE):
                self.light.enabled = not self.light.enabled

        # mouse_down is true every frame the button is held, so the cooldown inside
        # shoot() is what makes this a fire rate rather than one shot per frame.
        if Input.mouse_down(MouseButton.LEFT):
            self.shoot()


    def fixed_update(self): 
        if self.camera:
            self.camera.transform.position += (self.transform.position - self.camera.transform.position) * self.camera_smoothing * 0.01

        mouse_pos = Input.get_mouse_pos()



        input_ = Vector2(
            Input.is_key_down(Keys.D) - Input.is_key_down(Keys.A),
            Input.is_key_down(Keys.W) - Input.is_key_down(Keys.S)
        )

        direction = mouse_pos - self.transform.position
        self.rb.apply_impulse(input_.normalize() * self.speed * 100)
        self.rb.velocity *= self.friction
        self.transform.rotation = math.degrees(math.atan2(direction.y, direction.x))
        self.velocity = self.rb.velocity
        Debug.draw_circle(mouse_pos, 40)


    def shoot(self):
        """Hitscan shot: raycast to the first collider, run a particle tracer out to the
        hit point, and spray an impact burst back along the bounce direction."""
        now = Time.elapsed_time
        if now < self._next_shot:
            return
        self._next_shot = now + self.fire_rate

        origin = self.transform.world_position
        aim = Input.get_mouse_pos() - origin
        if aim.magnitude == 0.0:
            return                      # mouse exactly on the player; no direction to fire
        direction = aim.normalize()
        heading = math.degrees(math.atan2(direction.y, direction.x))

        if self.muzzle:
            self.muzzle.direction = heading
            self.muzzle.emit_burst(self.muzzle_count)

        # cast_ray's second argument is a TRANSLATION -- the full ray segment, handed
        # straight to b2World_CastRayClosest(origin, translation) -- not a heading. Passing
        # a normalised direction casts a ray one world unit long, which is why this used to
        # hit nothing but whatever was already overlapping the player.
        hit = Physics.cast_ray(origin, direction * self.fire_range)

        if hit:
            impact = hit.point
            self._spawn_impact(impact, self._reflect(direction, hit.normal))
            # Deliberately NO coordinates in this string. Console keys its message map on
            # the message text, so a unique string per call defeats the dedup counter and
            # leaks a Message plus a permanent Qt MessageGui widget on EVERY shot -- and
            # ConsoleGui rescans the whole map and relayouts synchronously inside
            # Engine::Update(), so the frame cost grows with every line logged. Naming only
            # the object keeps the key set bounded by how many things you can shoot, and
            # repeat hits just bump that message's count.
            Console.comment(f"Hit {hit.gameobject.name}")
        else:
            impact = origin + direction * self.fire_range

        # Scene-view only: DebugPass is installed by EditorRenderView and not by
        # GameRenderView, so this line is an authoring aid and is invisible in the Game
        # tab and in a shipped build. The particles below are the effect players see.
        Debug.draw_line(origin, impact, (1, 0.85, 0.2, 1))

        self.start_coroutine(self._tracer(origin, impact))

    @staticmethod
    def _reflect(incident, normal):
        """Mirror `incident` about the surface `normal` -- the classic R = D - 2(D.N)N.

        It is the DOT product, not the cross. D.N is how much of the incoming direction
        runs straight into the surface; subtracting twice that component flips exactly
        the part that hit the wall and leaves the sliding part alone, which is what makes
        a glancing shot spray sideways and a square-on shot spray back at you. (In 2D the
        cross product returns a scalar, not a vector -- it is the signed area used for
        winding and signed angles, so there is no reflection to be had from it.)

        Box2D hands back a unit normal pointing out of the surface toward the ray origin,
        so the result already points away from the wall.
        """
        n = normal.normalize()
        return incident - n * (2.0 * incident.dot(n))

    def _spawn_impact(self, point, bounce):
        """Burst at the hit point, aimed along the bounce."""
        if not self.bullet_impact:
            return
        self.bullet_impact.transform.world_position = point
        self.bullet_impact.direction = math.degrees(math.atan2(bounce.y, bounce.x))
        self.bullet_impact.spread = self.impact_spread
        self.bullet_impact.emit_burst(self.impact_count)

    def _tracer(self, start, end):
        """Walk the trail emitter from muzzle to impact, one step per frame.

        One burst per frame, and the emitter is deliberately LEFT at each step rather than
        moved back: EmitBurst only does `pendingBurst += count`, and those particles are
        not actually spawned until the particle system next updates, wherever the emitter
        happens to be by then. Emitting several bursts at different positions within a
        single frame -- or restoring the old position afterwards -- therefore spawns every
        one of them at the final position, which is the reason the old trail always came
        out in a clump at the player's feet.
        """
        if not self.bullet_trail:
            return

        segment = end - start
        steps = max(1, int(self.tracer_steps))

        for i in range(1, steps + 1):
            self.bullet_trail.transform.world_position = start + segment * (i / steps)
            self.bullet_trail.emit_burst(self.tracer_density)
            # Coroutines are ticked from every lifecycle method this class defines, and
            # this one defines both update and fixed_update -- so a frame advances this
            # loop twice. Steps, not seconds, so the tracer stays predictable either way.
            yield WaitForFrames(1)

    @action
    def Test_Function(self, val : str):
        Console.comment(f'{val} : {Time.elapsed_time}')
 