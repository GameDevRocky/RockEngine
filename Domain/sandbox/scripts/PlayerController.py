from Domain import *



class PlayerController(ScriptableComponent):
    camera : Camera
    speed : float = 500
    jump_height : float = 20
    ground_check : Transform = None
    direction : Vector2 = Vector2(0, -1)
    position : Vector2
    test_list : list[str]


    def awake(self):
        self.rb = self.get_component(Rigidbody)
        self.sr = self.get_component(SpriteRenderer)
        if not self.ground_check:
            self.ground_check = self.instantiate("Ground Check")
            self.ground_check.transform.parent = self.transform

    def update(self):
        
        self.position = self.transform.position
        Debug.draw_line(self.ground_check.transform.world_position, self.ground_check.transform.world_position + self.direction)
        


    def fixed_update(self):

        _input = Input.is_key_down(Keys.D) - Input.is_key_down(Keys.A)
        self.rb.apply_force((_input * self.speed, 0))
        grounded = False
        result = Physics.cast_ray(self.ground_check.transform.world_position, self.direction)
        if result:
            grounded = True
             
        if grounded and Input.is_key_down(Keys.W):
            self.rb.apply_impulse(Vector2(0, self.jump_height))




        if self.camera:
            self.camera.transform.position += (self.transform.position - self.camera.transform.position) * 0.05


