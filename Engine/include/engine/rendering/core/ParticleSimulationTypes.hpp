#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

// Binary layout of one particle in the OpenGL std430 SSBO.
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

// One std430 descriptor per emitter. The batched GLSL simulation indexes this
// table through a parallel owner buffer, allowing every active emitter to be
// advanced by a single compute dispatch while retaining independent settings.
struct alignas(16) ParticleEmitterGpuData {
    // particle offset, capacity, ring head, particles to emit this frame
    std::uint32_t meta[4] = {};
    // enabled, frame seed, simulation space, emission shape
    std::uint32_t flags[4] = {};
    // emitter position.xy, emitter rotation, cone angle
    float pose[4] = {};
    // shape size.xy, direction, spread
    float shapeMotion[4] = {};
    // speed min/max, lifetime min/max
    float speedLifetime[4] = {};
    // gravity.xy, damping, delta time
    float physics[4] = {};
};

static_assert(std::is_standard_layout_v<ParticleEmitterGpuData>);
static_assert(alignof(ParticleEmitterGpuData) == 16);
static_assert(sizeof(ParticleEmitterGpuData) == 96);
static_assert(offsetof(ParticleEmitterGpuData, meta) == 0);
static_assert(offsetof(ParticleEmitterGpuData, flags) == 16);
static_assert(offsetof(ParticleEmitterGpuData, pose) == 32);
static_assert(offsetof(ParticleEmitterGpuData, shapeMotion) == 48);
static_assert(offsetof(ParticleEmitterGpuData, speedLifetime) == 64);
static_assert(offsetof(ParticleEmitterGpuData, physics) == 80);
