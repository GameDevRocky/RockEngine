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


    def awake(self):
        self.rb = self.get_component(Rigidbody)
        self.rb.body_type = Rigidbody.DYNAMIC

        self.bc = self.get_component(BoxCollider)

    def update(self):
        if self.light:
            if Input.mouse_pressed(MouseButton.MIDDLE):
                self.light.enabled = not self.light.enabled

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
        """Fire a raycast in the direction of the mouse and spawn particles along the path."""
        # Emit muzzle flash
        if self.muzzle:
            self.muzzle.emit_burst(1)

        # Get shooting direction
        mouse_pos = Input.get_mouse_pos()
        direction = (mouse_pos - self.transform.position).normalize()

        # Cast ray
        ray_result = Physics.cast_ray(self.transform.position, direction)

        if ray_result:
            # Draw debug line to visualize raycast
            Debug.draw_line(self.transform.position, ray_result.point, (1, 0, 0, 1))

            # Spawn trail particles along the bullet path
            if self.bullet_trail:
                # Calculate number of particle bursts based on distance
                distance = (ray_result.point - self.transform.position).magnitude()
                num_particles = int(distance / 50)  # One burst every 50 units

                for i in range(max(1, num_particles)):
                    # Lerp position along the ray
                    t = (i + 1) / (num_particles + 1)
                    particle_pos = self.transform.position + direction * (distance * t)

                    # Temporarily move trail emitter and emit
                    old_pos = self.bullet_trail.transform.position
                    self.bullet_trail.transform.position = particle_pos
                    self.bullet_trail.emit_burst(3)
                    self.bullet_trail.transform.position = old_pos

            # Spawn impact particles at hit point
            if self.bullet_impact:
                old_pos = self.bullet_impact.transform.position
                self.bullet_impact.transform.position = ray_result.point
                self.bullet_impact.emit_burst(10)
                self.bullet_impact.transform.position = old_pos

            Console.comment(f"Hit: {ray_result.gameobject.name} at {ray_result.point}")
        else:
            # No hit - shoot to max range
            max_range = 2000
            end_point = self.transform.position + direction * max_range
            Debug.draw_line(self.transform.position, end_point, (0.5, 0.5, 0, 1))

    @action
    def Test_Function(self, val : str):
        Console.comment(f'{val} : {Time.elapsed_time}')
 