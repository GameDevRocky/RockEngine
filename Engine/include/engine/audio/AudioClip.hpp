#pragma once
#include <string>
#include "engine/rendering/core/Resource.hpp"
#include <yaml-cpp/yaml.h>

// A sound asset: a `.audio` meta pointing at a source file (wav/mp3/ogg/flac) on disk, in the
// same "meta next to source" convention as Texture2D/.texture and Font/.font (see
// AssetMetaService). Holds no decoded audio data itself -- AudioSource hands the path straight
// to miniaudio's resource manager (via AudioEngine's ma_engine), which does its own decode and
// caches/streams from the path, so multiple AudioSources playing the same clip don't each
// decode it separately.
class AudioClip : public Resource {
public:
    // Fired from the destructor (payload = id), mirroring Texture2D/Font, so UI holding a
    // reference (the asset-ref picker's preview) can drop it.
    static inline const Event DESTROYED_EVENT = AudioClip::CreateEvent();

    AudioClip() = default;
    ~AudioClip();

    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;

    // Probes duration/channels/sample rate by opening (and immediately closing) a decoder on
    // the source file. Cheap -- reads the container header only, not the audio data -- so it's
    // safe to do eagerly at load like Texture2D's decode, rather than lazily like Font's bake.
    void Awake() override;

    std::string GetTypeName() { return "AudioClip"; }
    void Accept(IVisitor* v) override;

    const std::string& GetPath() const { return path; }

    float GetDuration() const { return duration; }
    int GetChannels() const { return channels; }
    int GetSampleRate() const { return sampleRate; }

private:
    std::string path;   // absolute path to the source audio file

    // Probed on Awake(), never serialized -- always re-derived from the source file, same as
    // Texture2D's width/height.
    float duration = 0.0f;
    int channels = 0;
    int sampleRate = 0;
};
