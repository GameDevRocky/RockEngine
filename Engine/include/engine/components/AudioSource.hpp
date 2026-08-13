#pragma once
#include "engine/components/Component.hpp"
#include "engine/serialization/SerializableFactory.hpp"
#include "yaml-cpp/yaml.h"
#include <string>

// ma_sound is a large concrete struct defined in miniaudio.h (~4000 lines of the vendored
// header) -- forward-declared here as an opaque type so this widely-included header stays
// cheap. Only AudioSource.cpp includes <miniaudio.h> to actually touch it.
struct ma_sound;

// Unity-style AudioSource: plays an AudioClip with volume/pitch/loop/mute, optional 2D
// positional falloff (spatialBlend), and Play/Pause/Stop/PlayOneShot -- no mixers, no effects.
// Multiple AudioSources may be attached to one GameObject (not a Component::IsSingleton type),
// matching Unity.
//
// Owns a live miniaudio ma_sound created lazily against AudioEngine's single shared ma_engine
// (see AudioEngine.hpp for why that's a global singleton rather than a per-Container System).
// The ma_sound is runtime-only: Copy() carries only the authored fields below and leaves it
// null, exactly like RigidBody's b2BodyId -- play mode's container copy gets silent, freshly
// created sounds, never a shared live handle.
class AudioSource : public Component
{
public:
    static inline const Event CLIP_CHANGED_EVENT          = AudioSource::CreateEvent();
    static inline const Event VOLUME_CHANGED_EVENT         = AudioSource::CreateEvent();
    static inline const Event PITCH_CHANGED_EVENT          = AudioSource::CreateEvent();
    static inline const Event LOOP_CHANGED_EVENT           = AudioSource::CreateEvent();
    static inline const Event MUTE_CHANGED_EVENT           = AudioSource::CreateEvent();
    static inline const Event PLAY_ON_AWAKE_CHANGED_EVENT  = AudioSource::CreateEvent();
    static inline const Event SPATIAL_BLEND_CHANGED_EVENT  = AudioSource::CreateEvent();
    static inline const Event MIN_DISTANCE_CHANGED_EVENT   = AudioSource::CreateEvent();
    static inline const Event MAX_DISTANCE_CHANGED_EVENT   = AudioSource::CreateEvent();

    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;
    void Start() override;
    void Update() override;
    void OnDisabled() override;
    void Shutdown() override;

    AudioSource* Copy() override;
    void Accept(IVisitor* v) override;
    std::string GetTypeName() const override { return "AudioSource"; }

    const std::string& GetClipID() const { return clip_id; }
    void SetClip(const std::string& id);

    float GetVolume() const { return volume; }
    void SetVolume(float v);

    float GetPitch() const { return pitch; }
    void SetPitch(float v);

    bool GetLoop() const { return loop; }
    void SetLoop(bool v);

    bool GetMute() const { return mute; }
    void SetMute(bool v);

    bool GetPlayOnAwake() const { return playOnAwake; }
    void SetPlayOnAwake(bool v) { playOnAwake = v; Notify(PLAY_ON_AWAKE_CHANGED_EVENT); Notify(CHANGED_EVENT); }

    // 0 = always centered / full volume regardless of distance (a UI or music cue). 1 = fully
    // positional. Values between blend smoothly -- see UpdateSpatialPosition for how.
    float GetSpatialBlend() const { return spatialBlend; }
    void SetSpatialBlend(float v);

    float GetMinDistance() const { return minDistance; }
    void SetMinDistance(float v);

    float GetMaxDistance() const { return maxDistance; }
    void SetMaxDistance(float v);

    // Restarts the clip from the beginning, creating the live sound on first use.
    void Play();
    // Stops and rewinds to the start (Unity semantics: Stop() resets position).
    void Stop();
    // Stops without rewinding; UnPause() resumes from where it left off.
    void Pause();
    void UnPause();
    bool IsPlaying() const;

    // Fires an independent, untracked copy of this source's own clip at its current
    // volume/pitch/spatial settings, scaled by volumeScale. Does not touch or require the main
    // sound -- multiple overlapping one-shots (e.g. rapid-fire hits) are fine.
    void PlayOneShot(float volumeScale = 1.0f);

private:
    // (Re)creates m_sound bound to clip_id if it doesn't already point at that clip. No-op if
    // the clip can't be resolved.
    void EnsureSound();
    void DestroySound();
    // Pushes volume/pitch/loop/attenuation settings onto the live sound.
    void ApplySoundSettings();
    // Feeds miniaudio a position lerped between the listener's own position (spatialBlend == 0,
    // which cancels panning/attenuation entirely since distance becomes 0) and this
    // GameObject's true world position (spatialBlend == 1). Cheaper and more honest than a
    // separate fake volume/pan blend: real distance-based panning and attenuation both fall out
    // of miniaudio's own spatializer for free at any blend in between.
    void UpdateSpatialPosition();

    std::string clip_id = "";
    float volume = 1.0f;
    float pitch = 1.0f;
    bool loop = false;
    bool mute = false;
    bool playOnAwake = true;
    float spatialBlend = 0.0f;
    float minDistance = 100.0f;
    float maxDistance = 1500.0f;

    // ── Runtime only -- never serialized, never copied ──────────────────────
    ma_sound* m_sound = nullptr;
    std::string m_boundClipId;   // clip id the live m_sound was actually created from
};
