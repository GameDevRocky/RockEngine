from ...utils.re_math import Vector2
from .component_handler import Component

class Collider(Component):
    def __init__(self, obj_id= None):
        super().__init__(obj_id)
