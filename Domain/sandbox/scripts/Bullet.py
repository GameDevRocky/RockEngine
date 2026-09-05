"""A physics projectile that flies straight and dies on contact.

Spawned by cloning a disabled reference object in the scene -- see
TopDownController.shoot(). The shooter positions and activates the clone, then calls
launch(); everything else is this script's business.

── Why launch() does the setup instead of awake() ──────────────────────────────
The reference bullet sits in the scene INACTIVE. GameObject::SetActive disables every
component on the object, and GameObject::Awake()/Start() only visit components where
GetEnabled() is true -- so neither awake() nor start() ever runs, on the prefab or on a
clone of it. GameObject::Init() is NOT gated that way, which is the part that saves this:
RigidBody::Init calls b2CreateBody and ScriptComponent::Init creates this Python instance,
so a disabled prefab still has a real physics body and a live script for the shooter to
call into. Do not move this setup into awake() -- it will silently never run.
"""

from Domain import *


class Bullet(ScriptableComponent):

    speed : Reflect[float, Slider, Step(50), Range(50, 8000)] = 1800.0
    lifetime : Reflect[float, Slider, Step(0.1), Range(0.1, 30),
                       Tooltip("Seconds before the bullet expires if it never hits anything.")] = 3.0
    impact_count : Reflect[int, Slider, Step(1), Range(0, 64)] = 14
    impact_spread : Reflect[float, Slider, Step(5), Range(0, 180),
                            Tooltip("Cone width of the impact spray around the bounce direction.")] = 40.0

    # Deliberately unannotated: annotated names become serialized inspector fields, and
    # these are per-flight state. They are class attributes rather than set in awake()
    # because, as the module docstring explains, awake() never runs for these objects --
    # so these values are the defaults a bullet has before launch() is called.
    _owner_id = ""
    _die_at = 0.0
    _impact_emitter = None
    _pool = None

    def launch(self, direction, owner_id="", impact_emitter=None, pool=None):
        """Aim and fire. The shooter must already have set this object's world position
        and rotation, and activated it, BEFORE calling this.

        That order is load-bearing. RigidBody::OnTransformChanged treats any outside write
        to the Transform as a teleport and discards momentum on the axis that moved, so
        setting position after the velocity would leave the bullet sitting still at the
        muzzle with no clue why.
        """
        rb = self.get_component(Rigidbody)
        if rb is None:
            Console.warn("Bullet prefab has no Rigidbody -- it cannot move.")
            self.gameobject.destroy()
            return

        heading = direction.normalize()
        if heading.magnitude == 0.0:
            self.gameobject.destroy()
            return

        rb.body_type = Rigidbody.DYNAMIC
        rb.use_gravity = False              # top-down: a bullet should not arc
        rb.velocity = heading * self.speed

        self._owner_id = owner_id
        self._impact_emitter = impact_emitter
        self._pool = pool
        self._die_at = Time.elapsed_time + self.lifetime

    def update(self):
        # Expire bullets that never hit anything. Without this every miss is a live
        # GameObject with a Box2D body flying away forever, and they accumulate for as
        # long as the player keeps firing.
        if self._die_at and Time.elapsed_time >= self._die_at:
            self._return_to_pool()

    def on_collision_enter(self, other):
        # The bullet spawns just outside the shooter, but a player backing into their own
        # shot would otherwise kill it instantly.
        if self._owner_id and other.gameobject.id == self._owner_id:
            return

        self._spawn_impact()
        self._return_to_pool()

    def _return_to_pool(self):
        """Return this bullet to the pool for reuse, or destroy it if no pool is set."""
        # Stop the bullet's movement
        rb = self.get_component(Rigidbody)
        if rb:
            rb.velocity = Vector2(0, 0)

        # Reset state for next use
        self._owner_id = ""
        self._die_at = 0.0
        self._impact_emitter = None

        # Return to pool or destroy if pooling is not enabled
        if self._pool:
            self._pool.return_to_pool(self.gameobject)
        else:
            self.gameobject.destroy()

    # ── Impact ──────────────────────────────────────────────────────────────────
    def _spawn_impact(self):
        """Spray the shooter's impact emitter at our position, aimed along the bounce.

        The emitter belongs to the SHOOTER, not to the bullet, and is handed over in
        launch(). That is deliberate: an emitter living on the bullet would be destroyed
        together with it a line later, taking the impact burst with it before a single
        particle was drawn.
        """
        emitter = self._impact_emitter
        if not emitter or self.impact_count <= 0:
            return

        rb = self.get_component(Rigidbody)
        travel = rb.velocity if rb else Vector2(0, 0)
        if travel.magnitude == 0.0:
            return
        heading = travel.normalize()

        position = self.transform.world_position
        bounce = self._bounce(heading, position)

        emitter.transform.world_position = position
        emitter.direction = math.degrees(math.atan2(bounce.y, bounce.x))
        emitter.spread = self.impact_spread
        emitter.emit_burst(self.impact_count)

    def _bounce(self, heading, position):
        """Direction the debris should spray.

        on_collision_enter hands us the other Collider but no contact normal, so we go and
        find one: a short ray through the contact point reports the surface it crossed, and
        that normal gives a true reflection. If the ray misses (a corner, or the bullet is
        already fully inside), spraying straight back along the incoming direction is a
        reasonable stand-in.
        """
        margin = 12.0
        hit = Physics.cast_ray(position - heading * margin, heading * (margin * 2.0))
        if hit:
            return self._reflect(heading, hit.normal)
        return -heading

    @staticmethod
    def _reflect(incident, normal):
        """Mirror `incident` about the surface `normal` -- the classic R = D - 2(D.N)N.

        The DOT product, not the cross. D.N is how much of the incoming direction runs
        straight into the surface; subtracting twice that component flips exactly the part
        that hit the wall and leaves the sliding part alone, which is what makes a glancing
        shot spray sideways and a square-on shot spray back at you. (In 2D the cross
        product returns a scalar, not a vector -- the signed area used for winding and
        signed angles -- so there is no reflection to be had from it.)
        """
        n = normal.normalize()
        return incident - n * (2.0 * incident.dot(n))
