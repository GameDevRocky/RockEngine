from Domain import *
import math

class EnemyController(ScriptableComponent):
    player: GameObject = None
    speed: Reflect[float, Slider, Step(1), Range(1, 100)] = 50.0
    follow_distance: float = 200.0
    shoot_cooldown: float = 1.0
    
    rb: Reflect[Rigidbody, ReadOnly()]
    
    _timer = 0.0

    def awake(self):
        self.rb = self.get_component(Rigidbody)
        if not self.rb:
            self.rb = self.transform.gameobject.add_component(Rigidbody)
            self.rb.body_type = Rigidbody.DYNAMIC

    def update(self):
        pass

    def fixed_update(self):
        if self.player:
            # 1. Follow player
            direction = self.player.transform.position - self.transform.position
            dist = direction.magnitude
            
            if dist > self.follow_distance:
                # Move towards player
                self.rb.apply_impulse(direction.normalize() * self.speed * 100)
                
            # Friction to prevent infinite sliding
            self.rb.velocity *= 0.8
                
            # 2. Look at player
            self.transform.rotation = math.degrees(math.atan2(direction.y, direction.x))
            
            # 3. Shoot at player
            self._timer -= Time.fixed_delta_time
            if self._timer <= 0 and dist <= self.follow_distance + 100:
                self.shoot()
                self._timer = self.shoot_cooldown

    def _find_child_by_name(self, parent_transform, name):
        for child in parent_transform.children:
            if child.gameobject.name == name:
                return child.gameobject
        return None

    def shoot(self):
        # Find the weapon/muzzle and emit a particle burst
        body = self._find_child_by_name(self.transform, "Body")
        if body:
            weapon_holder = self._find_child_by_name(body.transform, "Wepon Holder")
            if weapon_holder:
                ar = self._find_child_by_name(weapon_holder.transform, "AR")
                if ar:
                    muzzle = self._find_child_by_name(ar.transform, "muzzel")
                    if muzzle:
                        particle = muzzle.get_component(ParticleComponent)
                        if particle:
                            particle.emit_burst(10)
                            return
                            
                    # Fallback to AR particle if muzzle doesn't exist
                    particle = ar.get_component(ParticleComponent)
                    if particle:
                        particle.emit_burst(10)
