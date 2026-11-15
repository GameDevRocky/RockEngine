from Domain import *


class Parallax(ScriptableComponent):
    """Moves a background layer by a fraction of the main camera's movement."""

    factor: Reflect[float, Slider, Step(0.05), Range(-2, 2),
                    Tooltip("0 stays in world space; 1 follows the camera completely.")] = 0.5
    horizontal: bool = True
    vertical: bool = True

    def start(self):
        self._camera = None
        self._layer_origin = Vector2(self.transform.world_position)
        self._camera_origin = Vector2(0, 0)
        self._resolve_camera()

    def late_update(self):
        if not self._camera and not self._resolve_camera():
            return

        camera_delta = self._camera.transform.world_position - self._camera_origin
        offset = Vector2(
            camera_delta.x * self.factor if self.horizontal else 0.0,
            camera_delta.y * self.factor if self.vertical else 0.0,
        )
        self.transform.world_position = self._layer_origin + offset

    def _resolve_camera(self):
        camera_id = Camera.main_gameobject_id()
        if not camera_id:
            return False

        self._camera = GameObject(camera_id)
        self._camera_origin = Vector2(self._camera.transform.world_position)
        return True
