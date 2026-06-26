from rock_engine.systems import debug_draw_module
from ...utils.re_math import Vector2, Vector3, Vector4

class Debug:
    @staticmethod
    def draw_point(pos : Vector2, color=(1, 1, 0, 1)):
        """Draw a point at pos (world space). color is (r, g, b, a) with values 0-1."""
        debug_draw_module.draw_point(Vector2(pos), tuple(color))

    @staticmethod
    def draw_line(start : Vector2, end : Vector2, color=(1, 1, 0, 1)):
        """Draw a line from start to end (world space)."""
        debug_draw_module.draw_line(Vector2(start), Vector2(end), tuple(color))

    @staticmethod
    def draw_box(center : Vector2, size : Vector2, rotation=0.0, color=(1, 1, 0, 1)):
        """Draw a box outline at center with given size (world space) and rotation (degrees)."""
        debug_draw_module.draw_box(Vector2(center), Vector2(size), float(rotation), tuple(color))

    @staticmethod
    def draw_circle(center : Vector2, radius : float, color : Vector4 = (1, 1, 0, 1) ):
        """Draw a circle outline at center with given radius (world space)."""
        debug_draw_module.draw_circle(Vector2(center), float(radius), tuple(color))

    @staticmethod
    def draw_points(points : list, closed : bool = False, filled : bool = False, color : Vector4 = (1, 1, 0, 1)):
        """Draw a polygon from a list of Vector2 points (world space).
        closed=True connects the last point back to the first.
        filled=True fills the interior of the polygon."""
        debug_draw_module.draw_points([Vector2(p) for p in points], closed, filled, tuple(color))
