from rock_engine.components import camera_module
from .component_handler import Component


class Camera(Component):
    _type_name = "Camera"

    def __init__(self, obj_id=None):
        super().__init__(obj_id)

    @staticmethod
    def main_gameobject_id() -> str:
        """GameObject id of the resolved main camera (highest-priority enabled
        Camera on an active GameObject), or "" if none exists."""
        return camera_module.get_main_camera_gameobject_id()

    @property
    def ortho_size(self) -> float:
        return camera_module.get_ortho_size(self._gameobject_id)

    @ortho_size.setter
    def ortho_size(self, value: float):
        camera_module.set_ortho_size(self._gameobject_id, float(value))

    @property
    def priority(self) -> int:
        return camera_module.get_priority(self._gameobject_id)

    @priority.setter
    def priority(self, value: int):
        camera_module.set_priority(self._gameobject_id, int(value))

    @property
    def clear_flags(self) -> int:
        """0 = SolidColor, 1 = DepthOnly, 2 = Nothing."""
        return camera_module.get_clear_flags(self._gameobject_id)

    @clear_flags.setter
    def clear_flags(self, value: int):
        camera_module.set_clear_flags(self._gameobject_id, int(value))

    @property
    def clear_color(self) -> list:
        return list(camera_module.get_clear_color(self._gameobject_id))

    @clear_color.setter
    def clear_color(self, value):
        r, g, b, a = value
        camera_module.set_clear_color(self._gameobject_id, float(r), float(g), float(b), float(a))

    @property
    def target_aspect(self) -> float:
        """<= 0 means free (fills the panel, no letterboxing)."""
        return camera_module.get_target_aspect(self._gameobject_id)

    @target_aspect.setter
    def target_aspect(self, value: float):
        camera_module.set_target_aspect(self._gameobject_id, float(value))

    @property
    def viewport_rect(self) -> list:
        return list(camera_module.get_viewport_rect(self._gameobject_id))

    @viewport_rect.setter
    def viewport_rect(self, value):
        x, y, w, h = value
        camera_module.set_viewport_rect(self._gameobject_id, float(x), float(y), float(w), float(h))

    @property
    def culling_layers(self) -> list:
        """Sorting-layer names this camera renders; empty means draw everything."""
        return camera_module.get_culling_layers(self._gameobject_id)

    @culling_layers.setter
    def culling_layers(self, value):
        camera_module.set_culling_layers(self._gameobject_id, list(value))

    @property
    def target_texture_id(self) -> str:
        return camera_module.get_target_texture_id(self._gameobject_id)

    @target_texture_id.setter
    def target_texture_id(self, value: str):
        camera_module.set_target_texture_id(self._gameobject_id, str(value))
