from Domain import *
import random
class TestScript(ScriptableComponent):
    speed: float = 400
    jump_force: float = 100
    scale : Reflect[float, Step(1)] = 32
    testVar : float = 0.1
    anotherVar : float = 0.0
    var : Reflect[float, Step(0.01)] = 0

    def init(self):
        self.inactive_pool : set[GameObject] = set()
        self.active_pool : set[GameObject] = set()
        self.sprite_renderer = self.get_component(SpriteRenderer)
        self.sprite_renderer.color = (1, 0, 0, 1)
        self.rb = self.get_component(Rigidbody)
        
        pass

    def awake(self):
        self.grounded = False

        for i in range(1):
            new_obj = self.instantiate("Ball")
            sr = new_obj.add_component(SpriteRenderer)
            sr.sprite = Sprite("sprite3")
            rb = new_obj.add_component(Rigidbody)
            cc = new_obj.add_component(CircleCollider)
            sr.color = Vector4(random.random(), random.random(), random.random(), 1.0)
            cc.friction = 0.5
            cc.bounciness = 1.0
            cc.density = 10
            new_obj.active = False
            self.inactive_pool.add(new_obj)

    def start(self):
        self.rb = self.get_component(Rigidbody)
        self.rb.enabled = False
        self.sprite_renderer = self.get_component(SpriteRenderer)
        self.sprite_renderer.color = (1, 0, 0, 1)

    def fixed_update(self):
        self.sprite_renderer.color = (1,1,1,1)
        self.sprite_renderer.set_uniform("uTime", self.var)
        self.pos = self.transform.position
        if Input.is_key_down(Keys.A):
            self.rb.apply_force((-self.speed, 0))
            self.sprite_renderer.flipX = True
        if Input.is_key_down(Keys.D):
            self.rb.apply_force((self.speed, 0))
            self.sprite_renderer.flipX = False
        if Input.is_key_down(Keys.SPACE):
            if self.inactive_pool:
                obj = self.inactive_pool.pop()
                obj.active = True
                pos = (0, 0)
                obj.transform.position = pos
                mouse_pos = Input.get_mouse_pos()
                shoot_dir = (mouse_pos - pos).normalize()
                obj.get_component(Rigidbody).apply_impulse(shoot_dir * 2000)
                self.active_pool.add(obj)

    def update(self):
        self.speed = 400
        recycled = set()
        for obj in self.active_pool:
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
        