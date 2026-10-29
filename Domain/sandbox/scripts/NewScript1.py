from Domain import *


class NewScript1(ScriptableComponent):
    """A new RockEngine script. Annotated class attributes become
    inspector fields, e.g. `speed: float = 5.0`.
    """

    balls : list[GameObject]

    def start(self):

        pass

    def update(self):
        if Input.mouse_down() or Input.is_key_down(Keys.SPACE):
            pos = Input.get_mouse_pos()
            if self.balls:
                ball : GameObject = random.choice(self.balls)
                new_ball = self.duplicate(ball)
                new_ball.transform.position = pos
        pass
