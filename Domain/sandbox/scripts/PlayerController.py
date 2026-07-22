from Domain import *


class PlayerController(ScriptableComponent):
    """Robust 2D platformer controller (DYNAMIC body): velocity-based movement with
    acceleration, coyote time, jump buffering, variable jump height, a self-excluding
    ground check, and slope-aligned movement. The physics solver owns gravity and
    collision; we set velocity. It drives the Animator by setting parameters -- the
    animator state machine (wired in the Animator editor) decides which clip plays.

    ── Editor setup ──────────────────────────────────────────────────────────
    Components on this GameObject: RigidBody (Dynamic, lock rotation on, gravity on),
      a Collider, SpriteRenderer, and an Animator.
    Animator PARAMETERS (the default scene uses these):
      yvel      (Float) -- vertical velocity; rising -> Jump, falling -> Fall
      grounded  (Bool)  -- on the ground? -> back to Idle
      Speed     (Float) -- horizontal speed; Idle <-> Run
    Input: A/D (or Left/Right) to move, Space or W to jump.
    Tune the numeric fields below in the inspector to match your world scale.
    """

    test_var : str

    # ── Movement tuning ──────────────────────────────────────────────────────
    move_speed: float = 200.0           # target horizontal speed
    acceleration: float = 400.0         # how quickly we reach move_speed
    deceleration: float = 500.0         # how quickly we stop when no input
    air_control: float = 0.7            # 0..1 fraction of accel/decel applied in the air

    # ── Jump tuning ──────────────────────────────────────────────────────────
    jump_velocity: float = 500.0        # upward speed applied on jump
    jump_cut_multiplier: float = 0.5    # velocity kept if the jump key is released early
    max_fall_speed: float = 700.0       # terminal downward speed
    coyote_time: float = 0.10           # grace to still jump just after leaving a ledge
    jump_buffer_time: float = 0.12      # grace to buffer a jump pressed just before landing

    # ── Ground check ─────────────────────────────────────────────────────────
    ground_check: Transform = None      # optional: assign a child at the feet; auto-created if empty
    ground_check_offset: float = 40.0   # how far below the origin to place the auto-created check
    ground_check_distance: float = 20.0 # length of the downward ground ray

    # ── Camera follow (optional) ─────────────────────────────────────────────
    camera: Camera = None
    camera_smoothing: float = 0.1

    def awake(self):
        self.rb = self.get_component(Rigidbody)
        self.sr = self.get_component(SpriteRenderer)
        self.animator = self.get_component(Animator)

        # A child at the feet is the ground-ray origin. Only spawn one if the
        # designer didn't wire an explicit ground_check in the inspector.
        if not self.ground_check:
            check = self.instantiate("Ground Check")
            check.transform.parent = self.transform
            check.transform.position = Vector2(0, -self.ground_check_offset)  # local, at the feet
            self.ground_check = check.transform

        self.facing = 1            # 1 = right, -1 = left
        self.grounded = False
        self.ground_normal = Vector2(0, 1)   # surface normal under the feet (flat by default)
        self.coyote_timer = 0.0
        self.jump_buffer_timer = 0.0
        self.is_jumping = False    # currently in the rising part of a jump (for jump-cut)

    # ── per-frame: buffer edge-triggered input + animation + camera ──────────
    def update(self):
        # is_key_pressed is a one-frame edge; read it here (per render frame), not
        # in fixed_update which can run zero or many times per frame and miss it.
        if Input.is_key_pressed(Keys.SPACE) or Input.is_key_pressed(Keys.W):
            self.jump_buffer_timer = self.jump_buffer_time
        self._update_animation()
        self._follow_camera()
    # ── fixed step: physics ──────────────────────────────────────────────────
    def fixed_update(self):
        dt = Time.fixed_delta_time
        if not self.rb:
            return
        self._check_ground(dt)
        self._move_horizontal(dt)
        self._handle_jump(dt)
        self._clamp_fall()

    # ── movement ─────────────────────────────────────────────────────────────
    def _move_horizontal(self, dt):
        move = self._axis(Keys.D, Keys.RIGHT) - self._axis(Keys.A, Keys.LEFT)

        vel = self.rb.velocity
        target = move * self.move_speed
        rate = self.acceleration if move != 0 else self.deceleration

        if self.grounded:
            # Move ALONG the ground surface (using the ray's surface normal) so
            # slopes are climbed at full speed and standing still doesn't slide.
            # On flat ground the tangent is (1, 0) -- identical to plain movement.
            n = self.ground_normal
            tangent = Vector2(n.y, -n.x)                    # perpendicular to the normal, +x-ish
            along = self._move_toward(vel.dot(tangent), target, rate * dt)
            vel = tangent * along
        else:
            # Airborne: steer x only; gravity owns y for a normal jump arc.
            vel.x = self._move_toward(vel.x, target, rate * self.air_control * dt)

        self.rb.velocity = vel

        # Face the movement direction (don't flip while idle).
        if move != 0:
            self.facing = 1 if move > 0 else -1
            if self.sr:
                self.sr.flipX = self.facing < 0

    def _check_ground(self, dt):
        origin = self.ground_check.world_position
        down = Vector2(0, -self.ground_check_distance)
        result = Physics.cast_ray(origin, down)
        # Ignore a hit on ourselves (ray starting inside our own collider).
        hit = bool(result and result.gameobject.id != self._gameobject_id)
        self.grounded = hit
        # Remember the surface normal so movement can follow the slope.
        self.ground_normal = result.normal if hit else Vector2(0, 1)

        if self.grounded:
            self.coyote_timer = self.coyote_time
            self.is_jumping = False
        else:
            self.coyote_timer = max(0.0, self.coyote_timer - dt)

        Debug.draw_line(origin, origin + down)

    def _handle_jump(self, dt):
        self.jump_buffer_timer = max(0.0, self.jump_buffer_timer - dt)

        # Jump if one was buffered and we're grounded (or within coyote time).
        if self.jump_buffer_timer > 0.0 and self.coyote_timer > 0.0:
            vel = self.rb.velocity
            vel.y = self.jump_velocity
            self.rb.velocity = vel
            self.jump_buffer_timer = 0.0
            self.coyote_timer = 0.0
            self.is_jumping = True
            if self.animator:
                self.animator.set_trigger('Jump')

        # Variable jump height: releasing the jump key while rising cuts the ascent.
        jump_held = Input.is_key_down(Keys.SPACE) or Input.is_key_down(Keys.W)
        if self.is_jumping and not jump_held:
            vel = self.rb.velocity
            if vel.y > 0.0:
                vel.y *= self.jump_cut_multiplier
                self.rb.velocity = vel
            self.is_jumping = False

    def _clamp_fall(self):
        vel = self.rb.velocity
        if vel.y < -self.max_fall_speed:
            vel.y = -self.max_fall_speed
            self.rb.velocity = vel

    # ── animation: just publish parameters; the state machine does the rest ──
    def _update_animation(self):
        if not self.animator or not self.rb:
            return
        vel = self.rb.velocity
        # Slope walking gives a vertical velocity while grounded; report 0 then so
        # Jump/Fall only trigger when actually airborne (or mid-jump), not on hills.
        yvel = vel.y if (not self.grounded or self.is_jumping) else 0.0
        # Names match the scene's Animator: yvel (Float) + grounded (Bool).
        self.animator.set_float('yvel', yvel)
        self.animator.set_bool('grounded', self.grounded)
        # Also published for the Run state (Speed = horizontal speed).
        self.animator.set_float('Speed', abs(vel.x))

    # ── camera ───────────────────────────────────────────────────────────────
    def _follow_camera(self):
        if self.camera:
            self.camera.transform.position += (
                (self.transform.position - self.camera.transform.position) * self.camera_smoothing)

    # ── helpers ──────────────────────────────────────────────────────────────
    @staticmethod
    def _axis(primary, secondary):
        return 1 if (Input.is_key_down(primary) or Input.is_key_down(secondary)) else 0

    @staticmethod
    def _move_toward(current, target, max_delta):
        """Step `current` toward `target` by at most `max_delta` (no overshoot)."""
        if current < target:
            return min(current + max_delta, target)
        return max(current - max_delta, target)
