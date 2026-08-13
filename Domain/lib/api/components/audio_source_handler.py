from rock_engine.components import audio_source_module
from .component_handler import Component
from ..audio.audio_clip_handler import AudioClip


class AudioSource(Component):
    """Unity-style AudioSource: plays an AudioClip with volume/pitch/loop/mute and optional 2D
    positional falloff. A GameObject may carry several (not a singleton component), so this is
    addressed by its own component id -- see Joint for the same pattern.
    """

    _type_name = "AudioSource"
    _addressed_by_component_id = True

    def __init__(self, obj_id=None, component_id=None):
        super().__init__(obj_id, component_id)

    @property
    def clip(self) -> AudioClip:
        clip_id = audio_source_module.get_clip_id(self._component_id)
        return AudioClip(clip_id) if clip_id else None

    @clip.setter
    def clip(self, value: AudioClip):
        clip_id = value.id if isinstance(value, AudioClip) else (value or "")
        audio_source_module.set_clip_id(self._component_id, str(clip_id))

    @property
    def volume(self) -> float:
        return audio_source_module.get_volume(self._component_id)

    @volume.setter
    def volume(self, value: float):
        audio_source_module.set_volume(self._component_id, float(value))

    @property
    def pitch(self) -> float:
        return audio_source_module.get_pitch(self._component_id)

    @pitch.setter
    def pitch(self, value: float):
        audio_source_module.set_pitch(self._component_id, float(value))

    @property
    def loop(self) -> bool:
        return audio_source_module.get_loop(self._component_id)

    @loop.setter
    def loop(self, value: bool):
        audio_source_module.set_loop(self._component_id, bool(value))

    @property
    def mute(self) -> bool:
        return audio_source_module.get_mute(self._component_id)

    @mute.setter
    def mute(self, value: bool):
        audio_source_module.set_mute(self._component_id, bool(value))

    @property
    def play_on_awake(self) -> bool:
        return audio_source_module.get_play_on_awake(self._component_id)

    @play_on_awake.setter
    def play_on_awake(self, value: bool):
        audio_source_module.set_play_on_awake(self._component_id, bool(value))

    @property
    def spatial_blend(self) -> float:
        """0 = always centered/full volume (a UI or music cue). 1 = fully positional."""
        return audio_source_module.get_spatial_blend(self._component_id)

    @spatial_blend.setter
    def spatial_blend(self, value: float):
        audio_source_module.set_spatial_blend(self._component_id, float(value))

    @property
    def min_distance(self) -> float:
        return audio_source_module.get_min_distance(self._component_id)

    @min_distance.setter
    def min_distance(self, value: float):
        audio_source_module.set_min_distance(self._component_id, float(value))

    @property
    def max_distance(self) -> float:
        return audio_source_module.get_max_distance(self._component_id)

    @max_distance.setter
    def max_distance(self, value: float):
        audio_source_module.set_max_distance(self._component_id, float(value))

    def play(self):
        audio_source_module.play(self._component_id)

    def stop(self):
        audio_source_module.stop(self._component_id)

    def pause(self):
        audio_source_module.pause(self._component_id)

    def unpause(self):
        audio_source_module.unpause(self._component_id)

    @property
    def is_playing(self) -> bool:
        return audio_source_module.is_playing(self._component_id)

    def play_one_shot(self, volume_scale: float = 1.0):
        audio_source_module.play_one_shot(self._component_id, float(volume_scale))
