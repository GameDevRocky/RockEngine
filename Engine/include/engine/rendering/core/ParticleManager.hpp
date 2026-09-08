#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <glm/glm.hpp>

#include "engine/rendering/core/ParticleSimulationTypes.hpp"

class ParticleComponent;

// Process-global owner of every emitter's GPU simulation state, living OUTSIDE
// any Container alongside Renderer/AssetManager (render resources have no
// per-world identity and must survive the editor/runtime swap). Owns the
// compute + draw programs, shared unit-quad VBO, and one global particle arena
// shared by every emitter. Emitters own stable slices of the arena and stable
// descriptor slots; a frame uploads all descriptors once and advances the
// whole arena with one fused GLSL compute dispatch.
//
// Simulation model (deliberately simple/robust): each emitter's arena slice is
// a fixed ring of particles. Emission writes new particles at a moving head;
// particles carry age/lifetime and are integrated by the fused compute pass;
// dead slots are culled in the draw vertex shader. No atomics or readback.
class ParticleManager
{
public:
    static ParticleManager& Get();

    // Context must be current. Compiles programs + builds the shared VBO once,
    // and wires the play-mode reset subscription. Idempotent.
    bool EnsureInitialized();

    // A simulation frame is collected first, then executed as one GPU batch.
    // Begin returns false when another viewport already simulated this frame.
    bool BeginSimulationFrame(float dt, std::uint64_t frameId);
    void QueueEmitter(ParticleComponent* emitter, const glm::vec2& emitterPos,
                      float emitterRot);
    void EndSimulationFrame();

    // Draw an emitter's particles. Its shared-arena slice must exist. The
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

    // Reclaim arena slices for emitters not touched since `frameId` (deleted
    // components, unloaded scenes). Context must be current.
    void GarbageCollect(std::uint64_t frameId);

    unsigned int GetQuadVBO() const { return quadVBO; }

private:
    ParticleManager() = default;
    ~ParticleManager() = default;
    ParticleManager(const ParticleManager&) = delete;
    ParticleManager& operator=(const ParticleManager&) = delete;

    struct EmitterState {
        std::uint32_t offset = 0;
        std::uint32_t capacity = 0;
        std::uint32_t descriptorSlot = 0;
        unsigned int head = 0;          // ring write cursor
        float emitAccumulator = 0.0f;   // fractional particles carried between frames
        float emitterTime = 0.0f;       // seconds since (re)start, for duration/loop
        std::uint32_t seed = 0;
        std::uint64_t lastTouchedFrame = 0;
        bool startBurstFired = false;
    };

    struct FreeRange {
        std::uint32_t offset = 0;
        std::uint32_t count = 0;
    };

    struct QueuedEmitter {
        EmitterState* state = nullptr;
        ParticleEmitterGpuData descriptor{};
    };

    EmitterState& GetOrCreate(ParticleComponent* emitter);
    void DestroyState(EmitterState& st);
    void ResetAll();
    void EnsureArenaCapacity(std::uint32_t required);
    void EnsureDescriptorCapacity(std::uint32_t required);
    std::uint32_t AcquireRange(std::uint32_t count);
    void ReleaseRange(std::uint32_t offset, std::uint32_t count);
    std::uint32_t AcquireDescriptorSlot();
    void ReleaseDescriptorSlot(std::uint32_t slot);
    void ClearArenaRange(std::uint32_t offset, std::uint32_t count,
                         std::uint32_t owner);
    void RecomputeArenaHighWater();

    bool initialized = false;
    bool pendingReset = false;   // serviced at the next context-current frame
    bool warnedCudaDeferred = false;

    unsigned int quadVBO = 0;
    unsigned int simProgram = 0;
    unsigned int drawProgram = 0;

    unsigned int particleSSBO = 0;
    unsigned int ownerSSBO = 0;
    unsigned int emitterSSBO = 0;
    std::uint32_t arenaCapacity = 0;
    std::uint32_t arenaHighWater = 0;
    std::uint32_t descriptorCapacity = 0;
    std::uint32_t descriptorHighWater = 0;

    float frameDt = 0.0f;
    std::uint64_t currentFrame = 0;
    std::uint64_t lastBatchedFrame = ~0ull;

    std::vector<FreeRange> freeRanges;
    std::vector<std::uint32_t> freeDescriptorSlots;
    std::vector<QueuedEmitter> queuedEmitters;
    std::vector<ParticleEmitterGpuData> descriptorUpload;

    std::unordered_map<std::string, EmitterState> states;
};
