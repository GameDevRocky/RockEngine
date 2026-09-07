import random

from Domain import *

from Raycaster import Raycaster
from ObjectPool import ObjectPool
from Bullet import Bullet

class TopDownController(ScriptableComponent):
    velocity : Reflect[Vector2, ReadOnly()] = Vector2(0,0)
    speed : Reflect[float, Slider, Step(1), Range(1, 100)] = 1.00
    friction : Reflect[float, Slider, Step(0.05), Range(0, 1)] = 0.8
    camera : Camera

    emit : Event
    light : Light = None

    camera_smoothing : Reflect[float, Slider, Step(0.1), Range(1, 10)] = 0.1

    rb : Reflect[Rigidbody, ReadOnly()]
    bc : Reflect[BoxCollider, ReadOnly()]
    muzzle : ParticleComponent
    bullet_impact : ParticleComponent = None

    # ── Weapon ──────────────────────────────────────────────────────────────
    # Point this at a GameObject with an ObjectPool script that manages bullet pooling.
    # The pool will be created in awake() and bullets will be reused instead of cloned.
    bullet_pool : GameObject = None

    fire_rate : Reflect[float, Slider, Step(0.02), Range(0.02, 1.0),
                        Tooltip("Seconds between shots while the button is held.")] = 0.12
    bullet_spread : Reflect[float, Slider, Step(1), Range(0, 180),
                            Tooltip("Total firing cone in degrees. Zero fires exactly at the cursor.")] = 0.0
    muzzle_offset : Reflect[float, Slider, Step(5), Range(0, 300),
                            Tooltip("How far in front of the player a bullet appears. Too small "
                                    "and it spawns inside your own collider.")] = 60.0
    muzzle_count : Reflect[int, Slider, Step(1), Range(0, 32)] = 4


    def awake(self):
        self.audio = self.get_component(AudioSource)
        self.rb = self.get_component(Rigidbody)
        
        self.rb.body_type = Rigidbody.DYNAMIC

        self.bc = self.get_component(BoxCollider)

        self._next_shot = 0.0

        # The impact emitter is teleported to each hit point and left there, so it must
        # simulate in world space. In LOCAL space every particle already alive is parented
        # to the emitter, so moving it would drag the last burst along to the new hit.
        if self.bullet_impact:
            self.bullet_impact.space = ParticleComponent.Space.WORLD

    def start(self):
        # Get reference to the bullet pool script
        if self.bullet_pool:
            self._pool_script = ScriptRef(self.bullet_pool.id, "ObjectPool")
            if not self._pool_script:
                Console.warn("TopDownController: bullet_pool GameObject has no ObjectPool script.")
        else:
            self._pool_script = None

    def update(self):
        if self.light:
            if Input.mouse_pressed(MouseButton.MIDDLE):
                self.light.enabled = not self.light.enabled

        # mouse_down is true every frame the button is held, so the cooldown inside
        # shoot() is what makes this a fire rate rather than one shot per frame.
        if Input.is_key_down(Keys.SPACE) or Input.mouse_down(MouseButton.LEFT):
            self.emit.invoke()


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

    @action
    def shoot(self):
        """Get a bullet from the pool, place it at the muzzle, and launch it."""
        now = Time.elapsed_time
        if now < self._next_shot:
            return
        self._next_shot = now + self.fire_rate

        if not self._pool_script:
            Console.warn("TopDownController: no bullet_pool assigned.")
            return

        origin = self.transform.world_position
        aim = Input.get_mouse_pos() - origin
        if aim.magnitude == 0.0:
            return                      # mouse exactly on the player; no direction to fire
        direction = aim.normalize()
        heading = math.degrees(math.atan2(direction.y, direction.x))

        # Treat spread as the total cone width, centered on the cursor. Build the
        # final direction from the perturbed angle so placement, visuals, and
        # launch velocity all agree on the same shot.
        if self.bullet_spread > 0.0:
            heading += random.uniform(-self.bullet_spread * 0.5,
                                      self.bullet_spread * 0.5)
            radians = math.radians(heading)
            direction = Vector2(math.cos(radians), math.sin(radians))

        if self.muzzle and self.muzzle_count > 0:
            self.muzzle.direction = heading
            self.muzzle.emit_burst(self.muzzle_count)

        # ObjectPool is generic -- it pools GameObjects and knows nothing about
        # Bullet -- so what comes back is the OBJECT. The script on it is fetched
        # below, once the object is positioned and live.
        obj = self._pool_script.get()
        if not obj:
            Console.warn("TopDownController: could not get bullet from pool.")
            return

        # Order matters, and getting it wrong produces a bullet that never moves.
        # RigidBody::OnTransformChanged treats an outside write to the Transform as a
        # teleport and discards momentum on the axis that moved -- so pose first, then
        # activate, and only then hand over to launch() to set the velocity.
        obj.transform.world_position = origin + direction * self.muzzle_offset
        obj.transform.world_rotation = heading
        obj.active = True
        self.audio.pitch = random.randint(150, 300) / 100
        self.audio.play_one_shot()

        # The Bullet script on the pooled object. get_component hands back a live
        # reference that re-resolves on every access, so it survives the
        # hot-reload that would invalidate a captured instance.
        bullet = obj.get_component(Bullet)
        if not bullet:
            Console.warn("TopDownController: pooled object has no Bullet script.")
            self._pool_script.return_to_pool(obj)
            return

        # The impact emitter stays OURS and is only borrowed: one living on the bullet
        # would be destroyed along with it before a single particle was drawn.
        # Pass the pool reference so the bullet can return itself when done.
        bullet.launch(direction, owner_id=self.gameobject.id,
                      impact_emitter=self.bullet_impact, pool=self._pool_script)

        
        

    @action
    def Test_Function(self, val : str):
        Console.comment(f'{val} : {Time.elapsed_time}')
