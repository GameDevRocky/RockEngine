from rock_engine.audio import audio_clip_module


class AudioClip:
    def __init__(self, clip_id: str):
        self.id = clip_id

    @property
    def name(self) -> str:
        return audio_clip_module.get_name(self.id)

    @property
    def duration(self) -> float:
        """Length in seconds."""
        return audio_clip_module.get_duration(self.id)

    @property
    def channels(self) -> int:
        return audio_clip_module.get_channels(self.id)

    @property
    def sample_rate(self) -> int:
        return audio_clip_module.get_sample_rate(self.id)
