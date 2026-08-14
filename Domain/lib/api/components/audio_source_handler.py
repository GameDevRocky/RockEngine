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
    def _cid(self) -> str:
        """This source's OWN component id, resolved lazily.

        add_component / get_components hand the handler its instance id up front, but a
        script's ``field : AudioSource`` reference does not: introspection.make_component_ref
        builds the handler from the owning GameObject's id alone, leaving _component_id None
        -- and every binding here is addressed by the component's own id. Resolving on use
        (rather than in make_component_ref) also keeps it correct when the reference is
        applied before the target object's components are attached.

        Returns "" rather than None when it cannot be resolved: the bindings take a
        std::string and no-op on an unknown id, so an unresolved source stays silent
        instead of raising a TypeError out of pybind's argument conversion.
        """
        return self._resolve_component_id() or ""

    @property
    def clip(self) -> AudioClip:
        clip_id = audio_source_module.get_clip_id(self._cid)
        return AudioClip(clip_id) if clip_id else None

    @clip.setter
    def clip(self, value: AudioClip):
        clip_id = value.id if isinstance(value, AudioClip) else (value or "")
        audio_source_module.set_clip_id(self._cid, str(clip_id))

    @property
    def volume(self) -> float:
        return audio_source_module.get_volume(self._cid)

    @volume.setter
    def volume(self, value: float):
        audio_source_module.set_volume(self._cid, float(value))

    @property
    def pitch(self) -> float:
        return audio_source_module.get_pitch(self._cid)

    @pitch.setter
    def pitch(self, value: float):
        audio_source_module.set_pitch(self._cid, float(value))

    @property
    def loop(self) -> bool:
        return audio_source_module.get_loop(self._cid)

    @loop.setter
    def loop(self, value: bool):
        audio_source_module.set_loop(self._cid, bool(value))

    @property
    def mute(self) -> bool:
        return audio_source_module.get_mute(self._cid)

    @mute.setter
    def mute(self, value: bool):
        audio_source_module.set_mute(self._cid, bool(value))

    @property
    def play_on_awake(self) -> bool:
        return audio_source_module.get_play_on_awake(self._cid)

    @play_on_awake.setter
    def play_on_awake(self, value: bool):
        audio_source_module.set_play_on_awake(self._cid, bool(value))

    @property
    def spatial_blend(self) -> float:
        """0 = always centered/full volume (a UI or music cue). 1 = fully positional."""
        return audio_source_module.get_spatial_blend(self._cid)

    @spatial_blend.setter
    def spatial_blend(self, value: float):
        audio_source_module.set_spatial_blend(self._cid, float(value))

    @property
    def min_distance(self) -> float:
        return audio_source_module.get_min_distance(self._cid)

    @min_distance.setter
    def min_distance(self, value: float):
        audio_source_module.set_min_distance(self._cid, float(value))

    @property
    def max_distance(self) -> float:
        return audio_source_module.get_max_distance(self._cid)

    @max_distance.setter
    def max_distance(self, value: float):
        audio_source_module.set_max_distance(self._cid, float(value))

    def play(self):
        audio_source_module.play(self._cid)

    def stop(self):
        audio_source_module.stop(self._cid)

    def pause(self):
        audio_source_module.pause(self._cid)

    def unpause(self):
        audio_source_module.unpause(self._cid)

    @property
    def is_playing(self) -> bool:
        return audio_source_module.is_playing(self._cid)

    def play_one_shot(self, volume_scale: float = 1.0):
        audio_source_module.play_one_shot(self._cid, float(volume_scale))
