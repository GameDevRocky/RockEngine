#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>

struct ma_engine;
struct ma_sound;

// Global audio output device + mixing graph (miniaudio's high-level ma_engine), analogous to
// Renderer/AssetManager: there is exactly one physical audio device for the whole app, shared
// by whichever Container is active -- audio hardware has no per-world identity, so this
// deliberately lives outside Container instead of being a per-Container System (see
// Engine/CLAUDE.md "Rendering" for the same reasoning applied to render resources). AudioSource
// components in either the editor or runtime container create their own ma_sound against this
// single engine; only one container ever ticks Update() at a time, so there is no double-play.
class AudioEngine {
public:
    static AudioEngine& Get();

    // Starts the miniaudio device + engine and sets up a 2D listener basis. Idempotent -- safe
    // to call every startup.
    void EnsureInitialized();
    void Shutdown();
    bool IsInitialized() const { return m_initialized; }

    ma_engine* GetEngine() const { return m_engine; }

    // Reaps finished fire-and-forget sounds started via PlayOneShot. Call once per frame.
    void Update();

    // Pulled once per frame (Engine::Update) from whichever AudioListener is active, falling
    // back to the main Camera -- see AudioListener::GetMain(). world.z is always 0 for a 2D
    // scene; the listener's forward/up basis is fixed at init so panning follows world-space X
    // and attenuation follows straight-line XY distance.
    void SetListenerPosition(const glm::vec3& worldPosition);
    const glm::vec3& GetListenerPosition() const { return m_listenerPos; }

    // Fire-and-forget playback not owned by any AudioSource (clip preview in the inspector,
    // AudioSource::PlayOneShot, UI blips). position/spatialBlend follow the same
    // listener<->source lerp AudioSource uses for its own long-lived sound; omit position for a
    // always-centered, non-positional one-shot.
    void PlayOneShot(const std::string& filePath, float volume = 1.0f, float pitch = 1.0f);
    void PlayOneShotAt(const std::string& filePath, const glm::vec3& worldPosition,
                        float spatialBlend, float volume = 1.0f, float pitch = 1.0f);

private:
    AudioEngine() = default;
    ~AudioEngine();
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    ma_sound* StartOneShot(const std::string& filePath, float volume, float pitch);

    ma_engine* m_engine = nullptr;
    bool m_initialized = false;
    glm::vec3 m_listenerPos = { 0.0f, 0.0f, 0.0f };

    // Sounds started by PlayOneShot*, freed once ma_sound_at_end() is true. Not touched by any
    // AudioSource -- those own their ma_sound directly for their whole lifetime.
    std::vector<ma_sound*> m_oneShots;
};
