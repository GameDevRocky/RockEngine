#include "engine/audio/AudioClip.hpp"
#include "engine/debug/Console.hpp"
#include "engine/utils/EngineUtils.hpp"
#include <miniaudio.h>

using namespace EngineUtils;

AudioClip::~AudioClip() {
    Notify(DESTROYED_EVENT, GetID());
}

YAML::Node AudioClip::Serialize() {
    YAML::Node node;
    node["type"] = "AudioClip";
    node["id"] = GetID();
    node["name"] = GetName();
    node["path"] = ToAssetRelative(path);
    return node;
}

void AudioClip::Deserialize(const YAML::Node& node) {
    Resource::Deserialize(node);
    path = GetAssetPath(node["path"].as<std::string>());
}

void AudioClip::Awake() {
    ma_decoder decoder;
    if (ma_decoder_init_file(path.c_str(), nullptr, &decoder) != MA_SUCCESS) {
        Console::Alert("AudioClip: failed to open '" + path + "'");
        return;
    }

    channels = static_cast<int>(decoder.outputChannels);
    sampleRate = static_cast<int>(decoder.outputSampleRate);

    ma_uint64 frameCount = 0;
    if (ma_decoder_get_length_in_pcm_frames(&decoder, &frameCount) == MA_SUCCESS && sampleRate > 0)
        duration = static_cast<float>(frameCount) / static_cast<float>(sampleRate);

    ma_decoder_uninit(&decoder);
}

void AudioClip::Accept(IVisitor* v) {
    v->Visit(this);
}
