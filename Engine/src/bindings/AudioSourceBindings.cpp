#include "engine/bindings/PythonBindings.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/components/AudioSource.hpp"
#include "Engine.hpp"

void BindAudioSource(pybind11::module_& m) {
    pybind11::module_ audio_source_module = m.def_submodule("audio_source_module", "AudioSource Bindings");

    // Every function below is addressed by the AudioSource's OWN id (not the GameObject's),
    // matching the joint/collider bindings -- a GameObject can carry several AudioSources.
    audio_source_module.def("get_clip_id", [](const std::string& id) -> std::string {
        if (auto* src = registry->Find<AudioSource>(id)) return src->GetClipID();
        return {};
    });
    audio_source_module.def("set_clip_id", [](const std::string& id, const std::string& clipId) {
        if (auto* src = registry->Find<AudioSource>(id)) src->SetClip(clipId);
    });

    audio_source_module.def("get_volume", [](const std::string& id) {
        auto* src = registry->Find<AudioSource>(id);
        return src ? src->GetVolume() : 1.0f;
    });
    audio_source_module.def("set_volume", [](const std::string& id, float v) {
        if (auto* src = registry->Find<AudioSource>(id)) src->SetVolume(v);
    });

    audio_source_module.def("get_pitch", [](const std::string& id) {
        auto* src = registry->Find<AudioSource>(id);
        return src ? src->GetPitch() : 1.0f;
    });
    audio_source_module.def("set_pitch", [](const std::string& id, float v) {
        if (auto* src = registry->Find<AudioSource>(id)) src->SetPitch(v);
    });

    audio_source_module.def("get_loop", [](const std::string& id) {
        auto* src = registry->Find<AudioSource>(id);
        return src && src->GetLoop();
    });
    audio_source_module.def("set_loop", [](const std::string& id, bool v) {
        if (auto* src = registry->Find<AudioSource>(id)) src->SetLoop(v);
    });

    audio_source_module.def("get_mute", [](const std::string& id) {
        auto* src = registry->Find<AudioSource>(id);
        return src && src->GetMute();
    });
    audio_source_module.def("set_mute", [](const std::string& id, bool v) {
        if (auto* src = registry->Find<AudioSource>(id)) src->SetMute(v);
    });

    audio_source_module.def("get_play_on_awake", [](const std::string& id) {
        auto* src = registry->Find<AudioSource>(id);
        return src && src->GetPlayOnAwake();
    });
    audio_source_module.def("set_play_on_awake", [](const std::string& id, bool v) {
        if (auto* src = registry->Find<AudioSource>(id)) src->SetPlayOnAwake(v);
    });

    audio_source_module.def("get_spatial_blend", [](const std::string& id) {
        auto* src = registry->Find<AudioSource>(id);
        return src ? src->GetSpatialBlend() : 0.0f;
    });
    audio_source_module.def("set_spatial_blend", [](const std::string& id, float v) {
        if (auto* src = registry->Find<AudioSource>(id)) src->SetSpatialBlend(v);
    });

    audio_source_module.def("get_min_distance", [](const std::string& id) {
        auto* src = registry->Find<AudioSource>(id);
        return src ? src->GetMinDistance() : 0.0f;
    });
    audio_source_module.def("set_min_distance", [](const std::string& id, float v) {
        if (auto* src = registry->Find<AudioSource>(id)) src->SetMinDistance(v);
    });

    audio_source_module.def("get_max_distance", [](const std::string& id) {
        auto* src = registry->Find<AudioSource>(id);
        return src ? src->GetMaxDistance() : 0.0f;
    });
    audio_source_module.def("set_max_distance", [](const std::string& id, float v) {
        if (auto* src = registry->Find<AudioSource>(id)) src->SetMaxDistance(v);
    });

    audio_source_module.def("play", [](const std::string& id) {
        if (auto* src = registry->Find<AudioSource>(id)) src->Play();
    });
    audio_source_module.def("stop", [](const std::string& id) {
        if (auto* src = registry->Find<AudioSource>(id)) src->Stop();
    });
    audio_source_module.def("pause", [](const std::string& id) {
        if (auto* src = registry->Find<AudioSource>(id)) src->Pause();
    });
    audio_source_module.def("unpause", [](const std::string& id) {
        if (auto* src = registry->Find<AudioSource>(id)) src->UnPause();
    });
    audio_source_module.def("is_playing", [](const std::string& id) {
        auto* src = registry->Find<AudioSource>(id);
        return src && src->IsPlaying();
    });
    audio_source_module.def("play_one_shot", [](const std::string& id, float volumeScale) {
        if (auto* src = registry->Find<AudioSource>(id)) src->PlayOneShot(volumeScale);
    });
}
