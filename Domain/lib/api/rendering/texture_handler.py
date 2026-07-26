from rock_engine.rendering import texture_module


class Texture2D:
    """Scripting handle for a Texture2D asset (keyed by asset id). Thin proxy
    over ``texture_module`` (C++). ``filter``/``wrap`` use the nested int
    constants below, matching the editor dropdowns."""

    class Filter:
        NEAREST, LINEAR = 0, 1

    class Wrap:
        REPEAT, CLAMP = 0, 1

    def __init__(self, texture_id: str):
        self.id = texture_id

    @property
    def width(self) -> int:
        return texture_module.get_width(self.id)

    @property
    def height(self) -> int:
        return texture_module.get_height(self.id)

    @property
    def path(self) -> str:
        return texture_module.get_path(self.id)

    @property
    def texture_id(self) -> int:
        """The underlying OpenGL texture handle."""
        return texture_module.get_texture_id(self.id)

    @property
    def filter(self) -> int:
        return texture_module.get_filter(self.id)

    @filter.setter
    def filter(self, value):
        texture_module.set_filter(self.id, int(value))

    @property
    def wrap(self) -> int:
        return texture_module.get_wrap(self.id)

    @wrap.setter
    def wrap(self, value):
        texture_module.set_wrap(self.id, int(value))
