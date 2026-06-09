from Domain import *
import random
import math

class TestScript(ScriptableComponent):
    speed: Reflect[int, Step(10)] = 200
    jump_force: float = 200
    scale : Reflect[float, Step(5)] = 32
    testVar : float
    sprite : Sprite = Sprite("sprite3")

 
    def init(self):
        self.inactive_pool : set[GameObject] = set()
        self.active_pool : set[GameObject] = set()
        self.sprite_renderer = self.get_component(SpriteRenderer)
        self.rb = self.get_component(Rigidbody) 
         
        pass

    def awake(self):
        self.grounded = False
        self.WATER_LEFT    = -300.0
        self.WATER_RIGHT   =  300.0
        self.WATER_LEVEL   = -100.0
        self.WATER_BOTTOM  = -300.0
        self.WATER_NODES   = 40
        self.W_SPRING      = 0.02
        self.W_RESTORE     = 0.025
        self.W_DAMPING     = 0.98
        self.W_MAX_VEL     = 5.0
        self.water_vel   = [0.0] * self.WATER_NODES
        self.water_nodes = []
        
    def generate(self):
        for i in range(100):
            new_obj = self.instantiate("Ball")
            sr = new_obj.add_component(SpriteRenderer)
            sr.sprite = self.sprite
            rb = new_obj.add_component(Rigidbody)
            cc = new_obj.add_component(CircleCollider)
            sr.color = Vector4(random.random(), random.random(), random.random(), 1.0)
            cc.friction = 0.5 
            cc.bounciness = 1.0
            cc.density = 10
            new_obj.active = False
            self.inactive_pool.add(new_obj)
            yield WaitForSeconds(0.5)

    def start(self):
        self.rb = self.get_component(Rigidbody)
        self.rb.enabled = False
        self.sprite_renderer = self.get_component(SpriteRenderer)
        self.gameobject.tag = "Enemy"
        width = self.WATER_RIGHT - self.WATER_LEFT
        for i in range(self.WATER_NODES):
            x = self.WATER_LEFT + width * i / (self.WATER_NODES - 1)
            node = self.instantiate("WaterNode")
            node.transform.position = Vector2(x, self.WATER_LEVEL)
            node.tag = "WaterNode"
            rb = node.add_component(Rigidbody)
            rb.body_type = Rigidbody.KINEMATIC
            rb.use_gravity = False
            rb.lock_rotation = True
            sr = node.add_component(SpriteRenderer)
            sr.sprite = Sprite("sprite3")
            sr.visible = False
            cc = node.add_component(CircleCollider)
            cc.is_sensor = True
            self.water_nodes.append(node)

    def on_trigger_enter(self, other: Collider):
        if GameObject(other._gameobject_id).tag == "WaterNode":
            for i, node in enumerate(self.water_nodes):
                if node.id == other._gameobject_id:
                    self.water_vel[i] = -self.W_MAX_VEL
                    break

    def fixed_update(self):
        self.transform.position = Input.get_mouse_pos()

    def turn_off(self):
        for obj in list(self.active_pool):
            sr = obj.get_component(SpriteRenderer)
            sr.visible = True
            yield

    def update(self):
        recycled = set()
        for obj in self.active_pool:
            obj.get_component(SpriteRenderer).sprite = self.sprite
            if obj.transform.position.y < -1000:
                obj.active = False
                recycled.add(obj)
        self.active_pool -= recycled
        self.inactive_pool |= recycled
        
        result = Physics.cast_ray(self.transform.position, -self.transform.up * self.scale)
        if result:
            self.grounded = True
        else:
            self.grounded = False
        if self.grounded and Input.is_key_down(Keys.W):
            self.rb.apply_impulse((0, self.jump_force))

    def late_update(self):
        triangle_radius = 60
        triangle = [Vector2(triangle_radius * math.cos(math.radians(90 + i * 120)),
                            triangle_radius * math.sin(math.radians(90 + i * 120)))
                    for i in range(3)]
        Debug.draw_points(triangle, closed=True, filled=True, color=(1.0, 0.4, 0.2, 0.6))

        if not self.water_nodes:
            return

        # Splash on E
        if Input.is_key_pressed(Keys.E):
            idx = random.randint(0, self.WATER_NODES - 1)
            self.water_vel[idx] = -self.W_MAX_VEL

        # Read current displacements from node transforms
        disp = [node.transform.position.y - self.WATER_LEVEL for node in self.water_nodes]

        # Spring physics
        for i in range(self.WATER_NODES):
            self.water_vel[i] += -self.W_RESTORE * disp[i]
            if i > 0:
                diff  = disp[i] - disp[i - 1]
                force = -self.W_SPRING * diff
                self.water_vel[i]     += force
                self.water_vel[i - 1] -= force
            if i < self.WATER_NODES - 1:
                diff  = disp[i] - disp[i + 1]
                force = -self.W_SPRING * diff
                self.water_vel[i]     += force
                self.water_vel[i + 1] -= force
            self.water_vel[i] = max(-self.W_MAX_VEL, min(self.W_MAX_VEL, self.water_vel[i])) * self.W_DAMPING
            disp[i] += self.water_vel[i]
            self.water_nodes[i].transform.position = Vector2(
                self.water_nodes[i].transform.position.x,
                self.WATER_LEVEL + disp[i]
            )

        points = [Vector2(self.WATER_LEFT, self.WATER_BOTTOM)]
        for node in self.water_nodes:
            points.append(node.transform.position)
        points.append(Vector2(self.WATER_RIGHT, self.WATER_BOTTOM))
        Debug.draw_points(points, closed=True, filled=True, color=(0.1, 0.3, 1.0, 1.0))