#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

// Binary layout shared by the OpenGL std430 SSBO and the CUDA kernels. Keep
// this free of GLM and CUDA headers so RockEngineCore's public headers remain
// usable when CUDA support is not compiled in.
struct alignas(8) ParticleGpuData {
    float position[2];
    float velocity[2];
    float age;
    float lifetime;
    float seed;
    float pad;
};

static_assert(std::is_standard_layout_v<ParticleGpuData>);
static_assert(alignof(ParticleGpuData) == 8);
static_assert(sizeof(ParticleGpuData) == 32);
static_assert(offsetof(ParticleGpuData, position) == 0);
static_assert(offsetof(ParticleGpuData, velocity) == 8);
static_assert(offsetof(ParticleGpuData, age) == 16);
static_assert(offsetof(ParticleGpuData, lifetime) == 20);
static_assert(offsetof(ParticleGpuData, seed) == 24);
static_assert(offsetof(ParticleGpuData, pad) == 28);

// Fully prepared, backend-neutral work for one emitter and one frame. The CPU
// owns emission timing and the ring head; both GPU implementations only execute
// this packet, preventing a fallback from consuming a burst twice.
struct ParticleSimulationStep {
    std::uint32_t emitCount = 0;
    std::uint32_t capacity = 0;
    std::uint32_t head = 0;
    std::uint32_t frameSeed = 0;

    int space = 0;
    int shape = 0;

    float emitterPosition[2] = {};
    float emitterRotation = 0.0f;
    float shapeSize[2] = {};
    float coneAngle = 0.0f;
    float direction = 0.0f;
    float spread = 0.0f;
    float speedRange[2] = {};
    float lifetimeRange[2] = {};

    float dt = 0.0f;
    float gravity[2] = {};
    float damping = 0.0f;
};
