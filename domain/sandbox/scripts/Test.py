from Domain import *
import random

class TestScript(ScriptableComponent):
    speed: Reflect[int, Step(10)] = 200
    jump_force: float = 200
    scale : Reflect[float, Step(5)] = 32
    testVar : float
    sprite : Sprite = Sprite("sprite3")
    test : int = "bitchboy"

 
    def init(self):
        self.inactive_pool : set[GameObject] = set()
        self.active_pool : set[GameObject] = set()
        self.sprite_renderer = self.get_component(SpriteRenderer)
        self.rb = self.get_component(Rigidbody) 
         
        pass

    def awake(self):
        self.grounded = False
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

    def start(self):
        self.rb = self.get_component(Rigidbody)
        self.rb.enabled = False
        self.sprite_renderer = self.get_component(SpriteRenderer)
        self.gameobject.tag = "Enemy"

    def fixed_update(self):
        self.sprite_renderer.color = (1,1,1,1)
        self.sprite_renderer.set_uniform("uTime", Time.elapsed_time)
        self.pos = self.transform.position

        if Input.is_key_pressed(Keys.P):
            self.gameobject.destroy()

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
        Debug.draw_line(self.transform.position, self.transform.position + -self.transform.up * self.scale)
        result = Physics.cast_ray(self.transform.position, -self.transform.up * self.scale)
        if result:
            self.grounded = True
        else:
            self.grounded = False
        if self.grounded and Input.is_key_down(Keys.W):
            self.rb.apply_impulse((0, self.jump_force))
