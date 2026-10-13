import math
import random

from Domain import *
from Bullet import Bullet


class EnemyController(ScriptableComponent):
    """Configurable ranged enemy driven by a small finite-state machine."""

    player: GameObject = None
    bullet_pool: GameObject = None
    bullet_impact: ParticleComponent = None

    speed: Reflect[float, Slider, Step(1), Range(1, 100)] = 50.0
    turn_speed: Reflect[float, Slider, Step(15), Range(15, 1080)] = 360.0
    patrol_radius: Reflect[float, Slider, Step(25), Range(50, 1000)] = 350.0
    detection_range: Reflect[float, Slider, Step(25), Range(50, 2000)] = 850.0
    disengage_range: Reflect[float, Slider, Step(25), Range(100, 3000)] = 1300.0
    attack_range: Reflect[float, Slider, Step(25), Range(50, 1500)] = 650.0
    preferred_range: Reflect[float, Slider, Step(25), Range(50, 1200)] = 425.0
    retreat_range: Reflect[float, Slider, Step(25), Range(25, 1000)] = 250.0
    shoot_cooldown: Reflect[float, Slider, Step(0.1), Range(0.1, 10)] = 1.0
    bullet_spread: Reflect[float, Slider, Step(1), Range(0, 90)] = 6.0
    muzzle_offset: Reflect[float, Slider, Step(5), Range(0, 500)] = 30.0
    search_duration: Reflect[float, Slider, Step(0.5), Range(0.5, 15)] = 4.0
    draw_ranges: Reflect[bool, Tooltip("Draw color-coded AI ranges during gameplay.")] = True

    rb: Reflect[Rigidbody, ReadOnly()]
    health: Reflect[float, Slider, Step(1), Range(0, 100), ReadOnly()] = 100.0
    state: Reflect[str, ReadOnly()] = "Idle"

    def awake(self):
        self.rb = self.get_component(Rigidbody)
        if not self.rb:
            self.rb = self.transform.gameobject.add_component(Rigidbody)
            self.rb.body_type = Rigidbody.DYNAMIC

        self.health = 100.0
        self.state = "Idle"
        self._spawn_position = Vector2(self.transform.position)
        self._patrol_target = Vector2(self._spawn_position)
        self._last_seen_position = Vector2(self._spawn_position)
        self._state_timer = random.uniform(0.25, 1.0)
        self._shoot_timer = random.uniform(0.0, self.shoot_cooldown)
        self._strafe_direction = random.choice((-1.0, 1.0))
        self._strafe_timer = random.uniform(0.8, 1.8)

    def start(self):
        self._pool_script = (ScriptRef(self.bullet_pool.id, "ObjectPool")
                             if self.bullet_pool else None)
        if not self._pool_script:
            Console.warn(f"{self.gameobject.name}: no bullet pool assigned.")

        if self.bullet_impact:
            self.bullet_impact.space = ParticleComponent.Space.WORLD

    def update(self):
        if not self.draw_ranges:
            return

        position = self.transform.world_position

        # Close-range behavior: retreat (red) and preferred spacing (orange).
        Debug.draw_circle(position, self.retreat_range, (1.0, 0.15, 0.1, 0.9))
        Debug.draw_circle(position, self.preferred_range, (1.0, 0.55, 0.05, 0.9))

        # Engagement behavior: attack (yellow), detection (green), and disengage (cyan).
        Debug.draw_circle(position, self.attack_range, (1.0, 0.9, 0.1, 0.9))
        Debug.draw_circle(position, self.detection_range, (0.15, 1.0, 0.25, 0.9))
        Debug.draw_circle(position, self.disengage_range, (0.1, 0.8, 1.0, 0.9))

        # Patrol is anchored to the spawn point instead of following the enemy.
        Debug.draw_circle(self._spawn_position, self.patrol_radius,
                          (0.7, 0.25, 1.0, 0.9))

    def fixed_update(self):
        if not self.rb:
            return

        dt = Time.fixed_delta_time
        self._shoot_timer = max(0.0, self._shoot_timer - dt)
        self._state_timer -= dt
        self._strafe_timer -= dt

        if not self.player:
            self._change_state("Return")
            self._update_return()
            self._dampen_velocity()
            return

        to_player = self.player.transform.position - self.transform.position
        distance = to_player.magnitude

        if distance <= self.detection_range:
            self._last_seen_position = Vector2(self.player.transform.position)

        if self.state in ("Idle", "Patrol", "Return"):
            if distance <= self.detection_range:
                self._change_state("Chase")
        elif self.state in ("Chase", "Attack"):
            if distance > self.disengage_range:
                self._change_state("Search")
            elif distance <= self.attack_range:
                self._change_state("Attack")
            else:
                self._change_state("Chase")
        elif self.state == "Search" and distance <= self.detection_range:
            self._change_state("Chase")

        if self.state == "Idle":
            self._update_idle()
        elif self.state == "Patrol":
            self._update_patrol()
        elif self.state == "Chase":
            self._update_chase(to_player)
        elif self.state == "Attack":
            self._update_attack(to_player, distance)
        elif self.state == "Search":
            self._update_search()
        else:
            self._update_return()

        if self.state in ("Chase", "Attack") and distance > 0.001:
            self._face(to_player)

        self._dampen_velocity()

    def late_update(self):
        if self.health <= 0:
            self.gameobject.destroy()

    def _change_state(self, new_state):
        if self.state == new_state:
            return

        self.state = new_state
        if new_state == "Idle":
            self._state_timer = random.uniform(0.5, 1.5)
        elif new_state == "Search":
            self._state_timer = self.search_duration
        elif new_state == "Patrol":
            self._choose_patrol_target()

    def _update_idle(self):
        if self._state_timer <= 0.0:
            self._change_state("Patrol")

    def _update_patrol(self):
        offset = self._patrol_target - self.transform.position
        if offset.magnitude < 45.0:
            self._change_state("Idle")
            return
        self._move(offset, 0.45)
        self._face(offset)

    def _update_chase(self, to_player):
        self._move(to_player, 1.0)

    def _update_attack(self, to_player, distance):
        if distance < self.retreat_range:
            self._move(to_player * -1.0, 1.0)
        elif distance > self.preferred_range * 1.15:
            self._move(to_player, 0.7)
        else:
            if self._strafe_timer <= 0.0:
                self._strafe_direction *= -1.0
                self._strafe_timer = random.uniform(0.8, 1.8)
            tangent = Vector2(-to_player.y, to_player.x) * self._strafe_direction
            self._move(tangent, 0.65)

        if self._shoot_timer <= 0.0:
            self.shoot()
            self._shoot_timer = self.shoot_cooldown * random.uniform(0.85, 1.15)

    def _update_search(self):
        offset = self._last_seen_position - self.transform.position
        if offset.magnitude > 55.0:
            self._move(offset, 0.65)
            self._face(offset)
        elif self._state_timer <= 0.0:
            self._change_state("Return")

    def _update_return(self):
        offset = self._spawn_position - self.transform.position
        if offset.magnitude < 55.0:
            self._change_state("Idle")
            return
        self._move(offset, 0.7)
        self._face(offset)

    def _choose_patrol_target(self):
        angle = random.uniform(0.0, math.tau)
        radius = random.uniform(self.patrol_radius * 0.35, self.patrol_radius)
        self._patrol_target = self._spawn_position + Vector2(
            math.cos(angle) * radius,
            math.sin(angle) * radius,
        )

    def _move(self, direction, strength):
        if direction.magnitude <= 0.001:
            return
        self.rb.apply_impulse(direction.normalize() * self.speed * 100.0 * strength)

    def _face(self, direction):
        if direction.magnitude <= 0.001:
            return

        target = math.degrees(math.atan2(direction.y, direction.x))
        current = self.transform.rotation
        delta = (target - current + 180.0) % 360.0 - 180.0
        max_step = self.turn_speed * Time.fixed_delta_time
        delta = max(-max_step, min(max_step, delta))
        self.transform.rotation = current + delta

    def _dampen_velocity(self):
        self.rb.velocity *= 0.82

    def _find_child_by_name(self, parent_transform, name):
        for child in parent_transform.children:
            if child.gameobject.name == name:
                return child.gameobject
        return None

    def shoot(self):
        if not self.player or not self._pool_script:
            return

        body = self._find_child_by_name(self.transform, "Body")
        weapon_holder = (self._find_child_by_name(body.transform, "Wepon Holder")
                         if body else None)
        ar = (self._find_child_by_name(weapon_holder.transform, "AR")
              if weapon_holder else None)
        muzzle = self._find_child_by_name(ar.transform, "muzzel") if ar else None

        particle = muzzle.get_component(ParticleComponent) if muzzle else None
        if not particle and ar:
            particle = ar.get_component(ParticleComponent)
        if particle:
            particle.emit_burst(10)

        origin = (muzzle.transform.world_position if muzzle
                  else self.transform.world_position)
        aim = self.player.transform.world_position - origin
        if aim.magnitude <= 0.001:
            return

        heading = math.degrees(math.atan2(aim.y, aim.x))
        heading += random.uniform(-self.bullet_spread * 0.5,
                                  self.bullet_spread * 0.5)
        radians = math.radians(heading)
        direction = Vector2(math.cos(radians), math.sin(radians))

        obj = self._pool_script.get()
        if not obj:
            Console.warn(f"{self.gameobject.name}: bullet pool returned no object.")
            return

        obj.transform.world_position = origin + direction * self.muzzle_offset
        obj.transform.world_rotation = heading
        obj.active = True

        bullet = obj.get_component(Bullet)
        if not bullet:
            Console.warn(f"{self.gameobject.name}: pooled object has no Bullet script.")
            self._pool_script.return_to_pool(obj)
            return

        bullet.launch(direction, owner_id=self.gameobject.id,
                      impact_emitter=self.bullet_impact, pool=self._pool_script)

    def on_collision_enter(self, other):
        if other.gameobject.tag == "Bullet":
            if self.health > 0:
                self.health -= 1
            self.rb.apply_impulse(self.transform.right * -1000)
            if self.player:
                self._last_seen_position = Vector2(self.player.transform.position)
                self._change_state("Chase")
