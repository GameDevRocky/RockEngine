#include "engine/bindings/PythonBindings.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/audio/AudioClip.hpp"

void BindAudioClip(pybind11::module_& m) {
    pybind11::module_ audio_clip_module = m.def_submodule("audio_clip_module", "AudioClip Bindings");

    audio_clip_module.def("get_name", [](const std::string& id) -> std::string {
        AudioClip* clip = AssetManager::Get().GetAudioClip(id);
        return clip ? clip->GetName() : "";
    });

    audio_clip_module.def("get_duration", [](const std::string& id) {
        AudioClip* clip = AssetManager::Get().GetAudioClip(id);
        return clip ? clip->GetDuration() : 0.0f;
    });

    audio_clip_module.def("get_channels", [](const std::string& id) {
        AudioClip* clip = AssetManager::Get().GetAudioClip(id);
        return clip ? clip->GetChannels() : 0;
    });

    audio_clip_module.def("get_sample_rate", [](const std::string& id) {
        AudioClip* clip = AssetManager::Get().GetAudioClip(id);
        return clip ? clip->GetSampleRate() : 0;
    });
}
