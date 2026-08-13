// The one translation unit that owns miniaudio's implementation -- every other file that
// touches ma_* types just includes <miniaudio.h> for declarations. Mirrors how
// STB_IMAGE_IMPLEMENTATION is confined to Texture2D.cpp.
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

// miniaudio pulls in <windows.h> on Windows for the WASAPI/WinMM/DSound backends, whose
// CreateEvent macro (-> CreateEventA/W) clobbers every Observable::CreateEvent() call parsed
// after this point -- including the static Event initializers in RuntimeObject.hpp, which is
// transitively included by nearly every engine header. Must be undefined before anything else
// in this file includes an engine header.
#ifdef _WIN32
#undef CreateEvent
#endif

#include "engine/audio/AudioEngine.hpp"
#include "engine/debug/Console.hpp"
#include <algorithm>

AudioEngine& AudioEngine::Get() {
    static AudioEngine instance;
    return instance;
}

AudioEngine::~AudioEngine() {
    Shutdown();
}

void AudioEngine::EnsureInitialized() {
    if (m_initialized) return;

#ifdef _WIN32
    // Claim STA on THIS thread before miniaudio can claim MTA on it. Order-independent by
    // design -- do not replace this with "just initialize audio later".
    //
    // miniaudio's Win32 context init calls CoInitializeEx(NULL, MA_COINIT_VALUE) on the
    // calling thread and holds that apartment for the context's whole lifetime, and
    // MA_COINIT_VALUE defaults to COINIT_MULTITHREADED. Windows OLE drag-and-drop --
    // RegisterDragDrop for drop targets and DoDragDrop under every QDrag::exec(), i.e. every
    // drag in the editor (Folder view assets, the sprite hover column, Hierarchy rows) --
    // requires the GUI thread to be STA. Whichever of Qt or miniaudio touches COM first wins
    // the apartment, and if miniaudio wins, Qt's OleInitialize() fails with RPC_E_CHANGED_MODE
    // and ALL drag-and-drop silently stops working while everything else keeps running.
    //
    // Requesting STA here makes us agree with Qt instead of racing it, so it no longer matters
    // whether Qt initializes OLE eagerly (at QApplication construction) or lazily (at the first
    // drop-target registration, which happens later than any sensible audio-init point).
    // miniaudio's own call then returns RPC_E_CHANGED_MODE, which it explicitly tolerates (it
    // just skips the matching CoUninitialize), and its audio thread still initializes its own
    // apartment separately -- so miniaudio's threading model is untouched.
    //
    // Deliberately not paired with CoUninitialize: the apartment is meant to last for process
    // life, exactly like the OleInitialize Qt itself never unwinds. A host that already chose
    // MTA gets RPC_E_CHANGED_MODE here and is left alone.
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
#endif

    m_engine = new ma_engine();
    ma_engine_config cfg = ma_engine_config_init();
    if (ma_engine_init(&cfg, m_engine) != MA_SUCCESS) {
        Console::Alert("AudioEngine: failed to initialize the audio device");
        delete m_engine;
        m_engine = nullptr;
        return;
    }

    // Fixed 2D listener basis: forward into the screen, world-up = +Y. With every source and
    // the listener always at z == 0, this makes miniaudio's stereo panning follow world-space X
    // and its distance attenuation follow straight-line XY distance -- exactly what a 2D scene
    // wants, with no per-frame orientation tracking needed.
    ma_engine_listener_set_direction(m_engine, 0, 0.0f, 0.0f, -1.0f);
    ma_engine_listener_set_world_up(m_engine, 0, 0.0f, 1.0f, 0.0f);

    m_initialized = true;
}

void AudioEngine::Shutdown() {
    if (!m_initialized) return;

    for (ma_sound* s : m_oneShots) {
        ma_sound_uninit(s);
        delete s;
    }
    m_oneShots.clear();

    ma_engine_uninit(m_engine);
    delete m_engine;
    m_engine = nullptr;
    m_initialized = false;
}

void AudioEngine::Update() {
    if (!m_initialized) return;

    m_oneShots.erase(std::remove_if(m_oneShots.begin(), m_oneShots.end(),
        [](ma_sound* s) {
            if (!ma_sound_at_end(s)) return false;
            ma_sound_uninit(s);
            delete s;
            return true;
        }), m_oneShots.end());
}

void AudioEngine::SetListenerPosition(const glm::vec3& worldPosition) {
    m_listenerPos = worldPosition;
    if (!m_initialized) return;
    ma_engine_listener_set_position(m_engine, 0, worldPosition.x, worldPosition.y, worldPosition.z);
}

ma_sound* AudioEngine::StartOneShot(const std::string& filePath, float volume, float pitch) {
    if (!m_initialized || filePath.empty()) return nullptr;

    ma_sound* sound = new ma_sound();
    if (ma_sound_init_from_file(m_engine, filePath.c_str(), 0, nullptr, nullptr, sound) != MA_SUCCESS) {
        Console::Alert("AudioEngine: failed to load '" + filePath + "'");
        delete sound;
        return nullptr;
    }

    ma_sound_set_volume(sound, volume);
    ma_sound_set_pitch(sound, pitch);
    ma_sound_start(sound);
    m_oneShots.push_back(sound);
    return sound;
}

void AudioEngine::PlayOneShot(const std::string& filePath, float volume, float pitch) {
    StartOneShot(filePath, volume, pitch);
}

void AudioEngine::PlayOneShotAt(const std::string& filePath, const glm::vec3& worldPosition,
                                 float spatialBlend, float volume, float pitch) {
    ma_sound* sound = StartOneShot(filePath, volume, pitch);
    if (!sound) return;

    // Same listener<->source position lerp AudioSource uses for its long-lived sound -- see the
    // comment on AudioSource::UpdateSpatialPosition for why this gives a real 2D/3D blend
    // instead of a fake volume-only approximation.
    ma_sound_set_spatialization_enabled(sound, MA_TRUE);
    ma_sound_set_attenuation_model(sound, ma_attenuation_model_linear);
    const glm::vec3 pos = glm::mix(m_listenerPos, worldPosition, std::clamp(spatialBlend, 0.0f, 1.0f));
    ma_sound_set_position(sound, pos.x, pos.y, pos.z);
}
