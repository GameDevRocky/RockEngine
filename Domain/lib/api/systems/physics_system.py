from rock_engine.systems import physics_module
from ...utils.re_math import Vector2
from ..core.gameobject_handler import GameObject

class RaycastResult:
    def __init__(self, point, normal, fraction, gameobject):
        self.point = Vector2(point)
        self.normal = Vector2(normal)
        self.fraction = fraction
        self.gameobject = gameobject


class Physics:
    @staticmethod
    def cast_ray(origin, direction):
        result = physics_module.cast_ray(origin, direction)
        if result is None:
            return None
        return RaycastResult(
            point=result["point"],
            normal=result["normal"],
            fraction=result["fraction"],
            gameobject= GameObject(result["game_object_id"])
        )
    