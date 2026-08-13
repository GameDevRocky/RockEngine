#include "engine/components/AudioSource.hpp"
#include "engine/audio/AudioEngine.hpp"
#include "engine/audio/AudioClip.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/components/Transform.hpp"
#include "engine/debug/Console.hpp"
#include <miniaudio.h>
#include <algorithm>

YAML::Node AudioSource::Serialize() {
    YAML::Node node = Component::Serialize();
    node["clip_id"] = clip_id;
    node["volume"] = volume;
    node["pitch"] = pitch;
    node["loop"] = loop;
    node["mute"] = mute;
    node["playOnAwake"] = playOnAwake;
    node["spatialBlend"] = spatialBlend;
    node["minDistance"] = minDistance;
    node["maxDistance"] = maxDistance;
    return node;
}

void AudioSource::Deserialize(const YAML::Node& node) {
    Component::Deserialize(node);
    clip_id = node["clip_id"].as<std::string>("");
    volume = node["volume"].as<float>(1.0f);
    pitch = node["pitch"].as<float>(1.0f);
    loop = node["loop"].as<bool>(false);
    mute = node["mute"].as<bool>(false);
    playOnAwake = node["playOnAwake"].as<bool>(true);
    spatialBlend = node["spatialBlend"].as<float>(0.0f);
    minDistance = node["minDistance"].as<float>(100.0f);
    maxDistance = node["maxDistance"].as<float>(1500.0f);
    state = State::Loaded;
}

void AudioSource::Start() {
    if (state >= State::Started) return;
    if (playOnAwake && enabled) Play();
    state = State::Started;
}

void AudioSource::Update() {
    if (m_sound) UpdateSpatialPosition();
}

void AudioSource::OnDisabled() {
    Component::OnDisabled();
    Pause();
}

void AudioSource::Shutdown() {
    DestroySound();
    Component::Shutdown();
}

void AudioSource::SetClip(const std::string& id) {
    if (clip_id == id) return;
    clip_id = id;
    Notify(CLIP_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void AudioSource::SetVolume(float v) {
    volume = std::clamp(v, 0.0f, 1.0f);
    if (m_sound) ma_sound_set_volume(m_sound, mute ? 0.0f : volume);
    Notify(VOLUME_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void AudioSource::SetPitch(float v) {
    pitch = v;
    if (m_sound) ma_sound_set_pitch(m_sound, pitch);
    Notify(PITCH_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void AudioSource::SetLoop(bool v) {
    loop = v;
    if (m_sound) ma_sound_set_looping(m_sound, loop ? MA_TRUE : MA_FALSE);
    Notify(LOOP_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void AudioSource::SetMute(bool v) {
    mute = v;
    if (m_sound) ma_sound_set_volume(m_sound, mute ? 0.0f : volume);
    Notify(MUTE_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void AudioSource::SetSpatialBlend(float v) {
    spatialBlend = std::clamp(v, 0.0f, 1.0f);
    UpdateSpatialPosition();
    Notify(SPATIAL_BLEND_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void AudioSource::SetMinDistance(float v) {
    minDistance = v;
    if (m_sound) ma_sound_set_min_distance(m_sound, minDistance);
    Notify(MIN_DISTANCE_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void AudioSource::SetMaxDistance(float v) {
    maxDistance = v;
    if (m_sound) ma_sound_set_max_distance(m_sound, maxDistance);
    Notify(MAX_DISTANCE_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void AudioSource::EnsureSound() {
    if (!AudioEngine::Get().IsInitialized()) return;
    if (m_sound && m_boundClipId == clip_id) return;   // already bound to the right clip

    DestroySound();

    AudioClip* clip = AssetManager::Get().GetAudioClip(clip_id);
    if (!clip) return;

    m_sound = new ma_sound();
    if (ma_sound_init_from_file(AudioEngine::Get().GetEngine(), clip->GetPath().c_str(),
                                 0, nullptr, nullptr, m_sound) != MA_SUCCESS) {
        Console::Alert("AudioSource: failed to load clip '" + clip->GetName() + "'");
        delete m_sound;
        m_sound = nullptr;
        return;
    }

    m_boundClipId = clip_id;
    ApplySoundSettings();
}

void AudioSource::DestroySound() {
    if (!m_sound) return;
    ma_sound_uninit(m_sound);
    delete m_sound;
    m_sound = nullptr;
    m_boundClipId.clear();
}

void AudioSource::ApplySoundSettings() {
    if (!m_sound) return;
    ma_sound_set_volume(m_sound, mute ? 0.0f : volume);
    ma_sound_set_pitch(m_sound, pitch);
    ma_sound_set_looping(m_sound, loop ? MA_TRUE : MA_FALSE);
    ma_sound_set_spatialization_enabled(m_sound, MA_TRUE);
    ma_sound_set_attenuation_model(m_sound, ma_attenuation_model_linear);
    ma_sound_set_min_distance(m_sound, minDistance);
    ma_sound_set_max_distance(m_sound, maxDistance);
    UpdateSpatialPosition();
}

void AudioSource::UpdateSpatialPosition() {
    if (!m_sound) return;
    Transform* t = GetTransform();
    if (!t) return;

    const glm::vec2 world = t->GetWorldPosition();
    const glm::vec3 listener = AudioEngine::Get().GetListenerPosition();
    const glm::vec3 pos = glm::mix(listener, glm::vec3(world, 0.0f), spatialBlend);
    ma_sound_set_position(m_sound, pos.x, pos.y, pos.z);
}

void AudioSource::Play() {
    EnsureSound();
    if (!m_sound) return;
    ma_sound_seek_to_pcm_frame(m_sound, 0);
    ma_sound_start(m_sound);
}

void AudioSource::Stop() {
    if (!m_sound) return;
    ma_sound_stop(m_sound);
    ma_sound_seek_to_pcm_frame(m_sound, 0);
}

void AudioSource::Pause() {
    if (m_sound) ma_sound_stop(m_sound);
}

void AudioSource::UnPause() {
    if (m_sound) ma_sound_start(m_sound);
}

bool AudioSource::IsPlaying() const {
    return m_sound && ma_sound_is_playing(m_sound);
}

void AudioSource::PlayOneShot(float volumeScale) {
    AudioClip* clip = AssetManager::Get().GetAudioClip(clip_id);
    if (!clip) return;

    const float vol = (mute ? 0.0f : volume) * volumeScale;
    Transform* t = GetTransform();
    const glm::vec3 pos = t ? glm::vec3(t->GetWorldPosition(), 0.0f) : glm::vec3(0.0f);
    AudioEngine::Get().PlayOneShotAt(clip->GetPath(), pos, spatialBlend, vol, pitch);
}

void AudioSource::Accept(IVisitor* v) {
    v->Visit(this);
}

AudioSource* AudioSource::Copy() {
    AudioSource* copy = new AudioSource();
    copy->id = id;
    copy->enabled = enabled;
    copy->gameobject_id = gameobject_id;
    copy->clip_id = clip_id;
    copy->volume = volume;
    copy->pitch = pitch;
    copy->loop = loop;
    copy->mute = mute;
    copy->playOnAwake = playOnAwake;
    copy->spatialBlend = spatialBlend;
    copy->minDistance = minDistance;
    copy->maxDistance = maxDistance;
    copy->state = State::Loaded;
    return copy;
}
