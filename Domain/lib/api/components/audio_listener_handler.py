from .component_handler import Component


class AudioListener(Component):
    """Marks the GameObject whose Transform drives the audio listener. No authored fields --
    pose is pulled from the Transform each frame by the engine. At most one per GameObject
    (mirrors Camera)."""

    _type_name = "AudioListener"
