from rock_engine.components import text_renderer_module
from .component_handler import Component


class TextHAlign:
    """Horizontal alignment. Values match the C++ TextHAlign enum order."""
    LEFT = 0
    CENTER = 1
    RIGHT = 2


class TextVAlign:
    """Vertical alignment. Values match the C++ TextVAlign enum order."""
    TOP = 0
    MIDDLE = 1
    BASELINE = 2
    BOTTOM = 3


class TextRenderer(Component):
    """World-space text drawn from an MSDF glyph atlas.

    Sits on a GameObject alongside a Transform, exactly like a SpriteRenderer,
    and sorts against sprites and particles by sorting layer + order.
    """

    _type_name = "TextRenderer"

    def __init__(self, obj_id=None):
        super().__init__(obj_id)

    @property
    def transform(self):
        return self.gameobject.transform

    @property
    def text(self) -> str:
        return text_renderer_module.get_text(self._gameobject_id)

    @text.setter
    def text(self, value):
        text_renderer_module.set_text(self._gameobject_id, str(value))

    @property
    def color(self) -> list:
        return text_renderer_module.get_color(self._gameobject_id)

    @color.setter
    def color(self, value):
        r, g, b, a = value
        text_renderer_module.set_color(self._gameobject_id,
                                       float(r), float(g), float(b), float(a))

    @property
    def font_size(self) -> float:
        return text_renderer_module.get_font_size(self._gameobject_id)

    @font_size.setter
    def font_size(self, value):
        text_renderer_module.set_font_size(self._gameobject_id, float(value))

    @property
    def font(self) -> str:
        """The font's asset NAME (e.g. "Nunito"), not its id.

        Assigning a name that no loaded font matches is ignored, so a typo leaves
        the current font in place rather than blanking the text.
        """
        return text_renderer_module.get_font(self._gameobject_id)

    @font.setter
    def font(self, value):
        text_renderer_module.set_font(self._gameobject_id, str(value))

    @property
    def visible(self) -> bool:
        return text_renderer_module.get_visible(self._gameobject_id)

    @visible.setter
    def visible(self, value):
        text_renderer_module.set_visible(self._gameobject_id, bool(value))

    @property
    def alignment(self) -> tuple:
        """(horizontal, vertical) -- see TextHAlign / TextVAlign."""
        return text_renderer_module.get_alignment(self._gameobject_id)

    @alignment.setter
    def alignment(self, value):
        h, v = value
        text_renderer_module.set_alignment(self._gameobject_id, int(h), int(v))

    @property
    def max_width(self):
        raise AttributeError("max_width is write-only from scripts")

    @max_width.setter
    def max_width(self, value):
        """Wrap width in world units. 0 disables wrapping."""
        text_renderer_module.set_max_width(self._gameobject_id, float(value))

    def set_outline(self, color, width):
        """Draw an outline behind the glyphs.

        `color` is an (r, g, b, a) tuple; `width` is in distance-field units,
        roughly 0..0.35. Pass width=0 to turn the outline off.
        """
        r, g, b, a = color
        text_renderer_module.set_outline(self._gameobject_id,
                                         float(r), float(g), float(b), float(a),
                                         float(width))

    def set_weight(self, weight):
        """Faux-bold. -0.25 thins the strokes, 0.25 bolds them, 0 is as designed."""
        text_renderer_module.set_weight(self._gameobject_id, float(weight))

    @property
    def sorting_order(self):
        raise AttributeError("sorting_order is write-only from scripts")

    @sorting_order.setter
    def sorting_order(self, value):
        text_renderer_module.set_sorting_order(self._gameobject_id, int(value))

    @property
    def sorting_layer(self):
        raise AttributeError("sorting_layer is write-only from scripts")

    @sorting_layer.setter
    def sorting_layer(self, value):
        text_renderer_module.set_sorting_layer(self._gameobject_id, str(value))
