import random

from Domain import *


class Test(ScriptableComponent):

    def __init__(self, obj_id=None):
        super().__init__(obj_id)

    def late_update(self):
        Debug.draw_line

    