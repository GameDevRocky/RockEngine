#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

class ParticleComponent;

// Process-global owner of every emitter's GPU simulation state, living OUTSIDE
// any Container alongside Renderer/AssetManager (render resources have no
// per-world identity and must survive the editor/runtime swap). Owns the
// compute + draw programs and the shared unit-quad VBO -- all shared across
// viewport GL contexts -- plus one particle SSBO per emitter, keyed by the
// emitter component's id.
//
// Simulation model (deliberately simple/robust): each emitter's SSBO is a fixed
// ring buffer of particles. Emission writes new particles at a moving head;
// particles carry age/lifetime and are integrated by a compute pass; dead slots
// are culled in the draw vertex shader. No free-lists, no atomics, no readback.
class ParticleManager
{
public:
    static ParticleManager& Get();

    // Context must be current. Compiles programs + builds the shared VBO once,
    // and wires the play-mode reset subscription. Idempotent.
    bool EnsureInitialized();

    // Advance one emitter by dt, exactly once per frame (guarded by frameId even
    // when several viewports draw the same frame). Creates/resizes GPU state as
    // needed. Context must be current. emitterPos/emitterRot are the resolved
    // world pose used for World-space emission.
    void Simulate(ParticleComponent* emitter, const glm::vec2& emitterPos,
                  float emitterRot, float dt, std::uint64_t frameId);

    // Draw an emitter's particles. Simulate() must have run this frame. The
    // caller owns the VAO (per-context) and the blend/depth state. `model` is
    // identity for World space or the emitter's world matrix for Local space.
    // `textureId` == 0 draws untextured (tinted quads).
    //
    // `uvScale`/`uvOffset` map the quad's [0,1] UVs onto the sprite's sub-rect
    // within its texture, so an atlas sprite shows only its own region rather
    // than the whole sheet; a negative scale component flips that axis. The
    // caller resolves them from the Sprite (see ScenePass) -- pass (1,1)/(0,0)
    // for a whole-texture, unflipped draw. This is per-emitter, so re-pointing an
    // emitter's sprite restyles its live particles too.
    void Draw(ParticleComponent* emitter, const glm::mat4& view, const glm::mat4& proj,
              const glm::mat4& model, unsigned int textureId,
              const glm::vec2& uvScale, const glm::vec2& uvOffset, unsigned int vao);

    // Free GPU state for emitters not touched since `frameId` (deleted
    // components, unloaded scenes). Context must be current.
    void GarbageCollect(std::uint64_t frameId);

    unsigned int GetQuadVBO() const { return quadVBO; }

private:
    ParticleManager() = default;
    ~ParticleManager() = default;
    ParticleManager(const ParticleManager&) = delete;
    ParticleManager& operator=(const ParticleManager&) = delete;

    struct EmitterState {
        unsigned int particleSSBO = 0;
        int capacity = 0;
        unsigned int head = 0;          // ring write cursor
        float emitAccumulator = 0.0f;   // fractional particles carried between frames
        float emitterTime = 0.0f;       // seconds since (re)start, for duration/loop
        std::uint32_t seed = 0;
        std::uint64_t lastSimulatedFrame = ~0ull;
        std::uint64_t lastTouchedFrame = 0;
        bool startBurstFired = false;
    };

    EmitterState& GetOrCreate(ParticleComponent* emitter);
    void DestroyState(EmitterState& st);
    void ResetAll();   // free all emitter buffers (keeps programs/VBO)

    bool initialized = false;
    bool pendingReset = false;   // set by play-mode events; serviced in Simulate (context-current)

    unsigned int quadVBO = 0;
    unsigned int emitProgram = 0;
    unsigned int simProgram = 0;
    unsigned int drawProgram = 0;

    std::unordered_map<std::string, EmitterState> states;
};
