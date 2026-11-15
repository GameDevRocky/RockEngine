import runpy
import sys
import unittest
from pathlib import Path
from types import ModuleType, SimpleNamespace
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[2]
Vector2 = runpy.run_path(str(ROOT / "Domain/lib/utils/re_math/vector2.py"))["Vector2"]


class PlayerControllerTest(unittest.TestCase):
    def test_airborne_wall_check_does_not_move_player(self):
        for direction in (-1, 1):
            with self.subTest(direction=direction):
                position = [10.0, 50.0]
                rays = []

                def get_position(_):
                    return tuple(position)

                def set_position(_, x, y):
                    position[:] = [x, y]

                def cast_ray(origin, ray_direction):
                    rays.append((origin.x, origin.y, ray_direction.x, ray_direction.y))
                    return None

                domain = ModuleType("Domain")
                for name in ("AudioSource", "Event", "Transform", "ParticleComponent", "Camera", "Sprite"):
                    setattr(domain, name, object)
                domain.ScriptableComponent = object
                domain.Vector2 = Vector2
                domain.PadButton = SimpleNamespace(CROSS=1)
                domain.action = lambda method: method
                domain.Physics = SimpleNamespace(cast_ray=cast_ray)
                domain.Debug = SimpleNamespace(draw_line=lambda *_: None)

                with patch.dict(sys.modules, {"Domain": domain}):
                    controller_class = runpy.run_path(
                        str(ROOT / "Domain/sandbox/scripts/PlayerController.py")
                    )["PlayerController"]

                controller = controller_class()
                controller.transform = SimpleNamespace(
                    world_position=Vector2._bound(get_position, set_position, "player")
                )
                controller.rb = SimpleNamespace(velocity=Vector2(0, -10))
                controller.grounded = False
                controller._gameobject_id = "player"
                controller._move_axis = lambda: direction
                controller._check_walls(1 / 60)

                self.assertEqual(position, [10.0, 50.0])
                self.assertEqual(rays, [(10.0, 70.0, direction * 20.0, 0.0)])


if __name__ == "__main__":
    unittest.main()
