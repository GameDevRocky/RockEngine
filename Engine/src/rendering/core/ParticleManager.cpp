#include "engine/rendering/core/ParticleManager.hpp"
#include "engine/components/ParticleComponent.hpp"
#include "engine/debug/FrameProfiler.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "Engine.hpp"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <new>
#include <vector>

namespace {

constexpr std::uint32_t kInitialArenaCapacity = 4096;
constexpr std::uint32_t kInitialDescriptorCapacity = 64;
constexpr std::uint32_t kLocalSize = 128;
constexpr std::uint32_t kUnowned = std::numeric_limits<std::uint32_t>::max();

const char* kParticleStructGLSL = R"(
struct Particle {
    vec2 position;
    vec2 velocity;
    float age;
    float lifetime;
    float seed;
    float _pad;
};
layout(std430, binding = 0) buffer Particles { Particle particles[]; };
)";

const char* kSimulationGLSL = R"(
layout(std430, binding = 1) readonly buffer ParticleOwners { uint owners[]; };

struct Emitter {
    uvec4 meta;           // arena offset, capacity, ring head, emit count
    uvec4 flags;          // enabled, frame seed, space, shape
    vec4 pose;            // position.xy, rotation, cone angle
    vec4 shapeMotion;     // shape size.xy, direction, spread
    vec4 speedLifetime;   // speed min/max, lifetime min/max
    vec4 physics;         // gravity.xy, damping, dt
};
layout(std430, binding = 2) readonly buffer Emitters { Emitter emitters[]; };

uniform uint uParticleHighWater;

const float PI = 3.14159265;
uint hash(uint x) { x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16; return x; }
float rnd(inout uint s) { s = hash(s); return float(s) * (1.0 / 4294967296.0); }
vec2 rot(vec2 v, float deg) {
    float r = radians(deg), c = cos(r), s = sin(r);
    return vec2(c * v.x - s * v.y, s * v.x + c * v.y);
}

void main() {
    uint particleIndex = gl_GlobalInvocationID.x;
    if (particleIndex >= uParticleHighWater) return;

    uint emitterIndex = owners[particleIndex];
    if (emitterIndex == 0xffffffffu) return;

    Emitter emitter = emitters[emitterIndex];
    if (emitter.flags.x == 0u) return;

    uint offset = emitter.meta.x;
    uint capacity = emitter.meta.y;
    uint localIndex = particleIndex - offset;
    if (particleIndex < offset || localIndex >= capacity) return;

    uint head = emitter.meta.z;
    uint emitCount = emitter.meta.w;
    uint emissionIndex = localIndex >= head
        ? localIndex - head
        : capacity - head + localIndex;

    Particle particle;
    if (emissionIndex < emitCount) {
        uint randomState = hash(emitter.flags.y + emissionIndex * 747796405u
                                + localIndex * 2891336453u);
        vec2 spawnOffset = vec2(0.0);
        float direction = emitter.shapeMotion.z;
        uint shape = emitter.flags.w;

        if (shape == 1u) {
            float angle = rnd(randomState) * 2.0 * PI;
            float radius = sqrt(rnd(randomState));
            spawnOffset = vec2(cos(angle), sin(angle)) * radius * emitter.shapeMotion.xy;
        } else if (shape == 2u) {
            spawnOffset = (vec2(rnd(randomState), rnd(randomState)) * 2.0 - 1.0)
                          * emitter.shapeMotion.xy;
        } else if (shape == 3u) {
            direction += (rnd(randomState) * 2.0 - 1.0) * emitter.pose.w * 0.5;
        }

        float speed = mix(emitter.speedLifetime.x, emitter.speedLifetime.y,
                          rnd(randomState));
        float lifetime = mix(emitter.speedLifetime.z, emitter.speedLifetime.w,
                             rnd(randomState));
        float spread = (rnd(randomState) * 2.0 - 1.0) * emitter.shapeMotion.w * 0.5;

        vec2 position = spawnOffset;
        vec2 velocity = rot(vec2(1.0, 0.0), direction + spread) * speed;
        if (emitter.flags.z == 1u) {
            position = emitter.pose.xy + rot(spawnOffset, emitter.pose.z);
            velocity = rot(velocity, emitter.pose.z);
        }

        particle.position = position;
        particle.velocity = velocity;
        particle.age = 0.0;
        particle.lifetime = max(lifetime, 0.0001);
        particle.seed = float(localIndex);
        particle._pad = 0.0;
    } else {
        particle = particles[particleIndex];
    }

    if (particle.age >= particle.lifetime) return;

    float dt = emitter.physics.w;
    particle.velocity += emitter.physics.xy * dt;
    particle.velocity *= exp(-emitter.physics.z * dt);
    particle.position += particle.velocity * dt;
    particle.age += dt;
    particles[particleIndex] = particle;
}
)";

const char* kDrawVert = R"(
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
uniform mat4 uView;
uniform mat4 uProj;
uniform mat4 uModel;
uniform vec4 uStartColor;
uniform vec4 uEndColor;
uniform float uStartSize;
uniform float uEndSize;
uniform vec2 uUVScale;
uniform vec2 uUVOffset;
out vec2 vUV;
out vec4 vColor;
void main() {
    Particle p = particles[gl_BaseInstance + gl_InstanceID];
    if (p.lifetime <= 0.0 || p.age >= p.lifetime) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        vUV = aUV;
        vColor = vec4(0.0);
        return;
    }
    float t = clamp(p.age / p.lifetime, 0.0, 1.0);
    float size = mix(uStartSize, uEndSize, t);
    vColor = mix(uStartColor, uEndColor, t);
    vec2 local = p.position + aPos * size;
    gl_Position = uProj * uView * uModel * vec4(local, 0.0, 1.0);
    vUV = aUV * uUVScale + uUVOffset;
}
)";

const char* kDrawFrag = R"(#version 460 core
in vec2 vUV;
in vec4 vColor;
uniform sampler2D uTexture;
uniform int uHasTexture;
out vec4 FragColor;
void main() {
    vec4 tex = (uHasTexture == 1) ? texture(uTexture, vUV) : vec4(1.0);
    FragColor = tex * vColor;
    if (FragColor.a <= 0.001) discard;
}
)";

GLuint CompileStage(GLenum type, const std::vector<const char*>& sources)
{
    const GLuint shader = glad_glCreateShader(type);
    glad_glShaderSource(shader, static_cast<GLsizei>(sources.size()), sources.data(), nullptr);
    glad_glCompileShader(shader);
    GLint ok = 0;
    glad_glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048]{};
        glad_glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "ParticleManager: shader compile failed:\n" << log << std::endl;
    }
    return shader;
}

GLuint LinkProgram(const std::vector<GLuint>& stages)
{
    const GLuint program = glad_glCreateProgram();
    for (GLuint stage : stages) glad_glAttachShader(program, stage);
    glad_glLinkProgram(program);
    GLint ok = 0;
    glad_glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048]{};
        glad_glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::cerr << "ParticleManager: program link failed:\n" << log << std::endl;
    }
    for (GLuint stage : stages) glad_glDeleteShader(stage);
    return program;
}

GLuint BuildCompute()
{
    const char* version = "#version 460 core\n";
    const char* localSize = "layout(local_size_x = 128) in;\n";
    return LinkProgram({ CompileStage(GL_COMPUTE_SHADER,
                                      { version, localSize, kParticleStructGLSL,
                                        kSimulationGLSL }) });
}

void SetU1i(GLuint p, const char* n, int v) { glad_glUniform1i(glad_glGetUniformLocation(p, n), v); }
void SetU1ui(GLuint p, const char* n, GLuint v) { glad_glUniform1ui(glad_glGetUniformLocation(p, n), v); }
void SetU1f(GLuint p, const char* n, float v) { glad_glUniform1f(glad_glGetUniformLocation(p, n), v); }
void SetU2f(GLuint p, const char* n, const glm::vec2& v) { glad_glUniform2f(glad_glGetUniformLocation(p, n), v.x, v.y); }
void SetU4f(GLuint p, const char* n, const glm::vec4& v) { glad_glUniform4f(glad_glGetUniformLocation(p, n), v.x, v.y, v.z, v.w); }
void SetUMat4(GLuint p, const char* n, const glm::mat4& m) { glad_glUniformMatrix4fv(glad_glGetUniformLocation(p, n), 1, GL_FALSE, glm::value_ptr(m)); }

} // namespace

ParticleManager& ParticleManager::Get()
{
    static ParticleManager instance;
    return instance;
}

bool ParticleManager::EnsureInitialized()
{
    if (initialized) return true;

    simProgram = BuildCompute();

    const char* version = "#version 460 core\n";
    const GLuint vs = CompileStage(GL_VERTEX_SHADER, { version, kParticleStructGLSL, kDrawVert });
    const GLuint fs = CompileStage(GL_FRAGMENT_SHADER, { kDrawFrag });
    drawProgram = LinkProgram({ vs, fs });

    const float quad[] = {
        -0.5f, -0.5f, 0.0f, 0.0f,
         0.5f, -0.5f, 1.0f, 0.0f,
         0.5f,  0.5f, 1.0f, 1.0f,
        -0.5f, -0.5f, 0.0f, 0.0f,
         0.5f,  0.5f, 1.0f, 1.0f,
        -0.5f,  0.5f, 0.0f, 1.0f,
    };
    glad_glGenBuffers(1, &quadVBO);
    glad_glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glad_glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glad_glBindBuffer(GL_ARRAY_BUFFER, 0);

    Engine::Get()->Subscribe([this]() { pendingReset = true; return true; },
                             Engine::ENTER_PLAY_MODE_EVENT);
    Engine::Get()->Subscribe([this]() { pendingReset = true; return true; },
                             Engine::EXIT_PLAY_MODE_EVENT);

    initialized = true;
    return true;
}

void ParticleManager::EnsureArenaCapacity(std::uint32_t required)
{
    if (required <= arenaCapacity) return;

    std::uint32_t newCapacity = std::max(kInitialArenaCapacity, arenaCapacity);
    while (newCapacity < required) {
        const std::uint64_t doubled = static_cast<std::uint64_t>(newCapacity) * 2u;
        newCapacity = static_cast<std::uint32_t>(std::min<std::uint64_t>(
            doubled, std::numeric_limits<std::uint32_t>::max()));
        if (newCapacity < required && newCapacity == std::numeric_limits<std::uint32_t>::max())
            throw std::bad_alloc();
    }

    GLuint newParticles = 0;
    glad_glGenBuffers(1, &newParticles);
    glad_glBindBuffer(GL_SHADER_STORAGE_BUFFER, newParticles);
    glad_glBufferData(GL_SHADER_STORAGE_BUFFER,
                      static_cast<GLsizeiptr>(newCapacity) * sizeof(ParticleGpuData),
                      nullptr, GL_DYNAMIC_DRAW);
    const GLuint zero = 0;
    glad_glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER,
                           GL_UNSIGNED_INT, &zero);

    GLuint newOwners = 0;
    glad_glGenBuffers(1, &newOwners);
    glad_glBindBuffer(GL_SHADER_STORAGE_BUFFER, newOwners);
    glad_glBufferData(GL_SHADER_STORAGE_BUFFER,
                      static_cast<GLsizeiptr>(newCapacity) * sizeof(std::uint32_t),
                      nullptr, GL_DYNAMIC_DRAW);
    glad_glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER,
                           GL_UNSIGNED_INT, &kUnowned);

    if (arenaCapacity > 0) {
        glad_glBindBuffer(GL_COPY_READ_BUFFER, particleSSBO);
        glad_glBindBuffer(GL_COPY_WRITE_BUFFER, newParticles);
        glad_glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0,
            static_cast<GLsizeiptr>(arenaCapacity) * sizeof(ParticleGpuData));

        glad_glBindBuffer(GL_COPY_READ_BUFFER, ownerSSBO);
        glad_glBindBuffer(GL_COPY_WRITE_BUFFER, newOwners);
        glad_glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0,
            static_cast<GLsizeiptr>(arenaCapacity) * sizeof(std::uint32_t));
    }

    if (particleSSBO) glad_glDeleteBuffers(1, &particleSSBO);
    if (ownerSSBO) glad_glDeleteBuffers(1, &ownerSSBO);
    particleSSBO = newParticles;
    ownerSSBO = newOwners;

    const std::uint32_t oldCapacity = arenaCapacity;
    arenaCapacity = newCapacity;
    ReleaseRange(oldCapacity, newCapacity - oldCapacity);

    glad_glBindBuffer(GL_COPY_READ_BUFFER, 0);
    glad_glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    glad_glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void ParticleManager::EnsureDescriptorCapacity(std::uint32_t required)
{
    if (required <= descriptorCapacity) return;
    std::uint32_t newCapacity = std::max(kInitialDescriptorCapacity, descriptorCapacity);
    while (newCapacity < required) newCapacity *= 2u;

    if (!emitterSSBO) glad_glGenBuffers(1, &emitterSSBO);
    glad_glBindBuffer(GL_SHADER_STORAGE_BUFFER, emitterSSBO);
    glad_glBufferData(GL_SHADER_STORAGE_BUFFER,
                      static_cast<GLsizeiptr>(newCapacity) * sizeof(ParticleEmitterGpuData),
                      nullptr, GL_STREAM_DRAW);
    glad_glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    descriptorCapacity = newCapacity;
}

std::uint32_t ParticleManager::AcquireRange(std::uint32_t count)
{
    if (arenaCapacity == 0) EnsureArenaCapacity(count);

    auto findRange = [&]() {
        return std::find_if(freeRanges.begin(), freeRanges.end(),
                            [count](const FreeRange& range) { return range.count >= count; });
    };

    auto range = findRange();
    if (range == freeRanges.end()) {
        EnsureArenaCapacity(arenaCapacity + count);
        range = findRange();
    }

    const std::uint32_t offset = range->offset;
    range->offset += count;
    range->count -= count;
    if (range->count == 0) freeRanges.erase(range);
    arenaHighWater = std::max(arenaHighWater, offset + count);
    return offset;
}

void ParticleManager::ReleaseRange(std::uint32_t offset, std::uint32_t count)
{
    if (count == 0) return;
    freeRanges.push_back({ offset, count });
    std::sort(freeRanges.begin(), freeRanges.end(),
              [](const FreeRange& a, const FreeRange& b) { return a.offset < b.offset; });

    std::vector<FreeRange> merged;
    merged.reserve(freeRanges.size());
    for (const FreeRange& range : freeRanges) {
        if (!merged.empty() && merged.back().offset + merged.back().count >= range.offset) {
            const std::uint32_t end = std::max(merged.back().offset + merged.back().count,
                                               range.offset + range.count);
            merged.back().count = end - merged.back().offset;
        } else {
            merged.push_back(range);
        }
    }
    freeRanges.swap(merged);
    RecomputeArenaHighWater();
}

std::uint32_t ParticleManager::AcquireDescriptorSlot()
{
    std::uint32_t slot = 0;
    if (!freeDescriptorSlots.empty()) {
        slot = freeDescriptorSlots.back();
        freeDescriptorSlots.pop_back();
    } else {
        slot = descriptorHighWater++;
    }
    EnsureDescriptorCapacity(slot + 1);
    return slot;
}

void ParticleManager::ReleaseDescriptorSlot(std::uint32_t slot)
{
    freeDescriptorSlots.push_back(slot);
}

void ParticleManager::ClearArenaRange(std::uint32_t offset, std::uint32_t count,
                                      std::uint32_t owner)
{
    if (count == 0) return;
    glad_glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSSBO);
    const GLuint zero = 0;
    glad_glClearBufferSubData(GL_SHADER_STORAGE_BUFFER, GL_R32UI,
        static_cast<GLintptr>(offset) * sizeof(ParticleGpuData),
        static_cast<GLsizeiptr>(count) * sizeof(ParticleGpuData),
        GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);

    glad_glBindBuffer(GL_SHADER_STORAGE_BUFFER, ownerSSBO);
    glad_glClearBufferSubData(GL_SHADER_STORAGE_BUFFER, GL_R32UI,
        static_cast<GLintptr>(offset) * sizeof(std::uint32_t),
        static_cast<GLsizeiptr>(count) * sizeof(std::uint32_t),
        GL_RED_INTEGER, GL_UNSIGNED_INT, &owner);
    glad_glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void ParticleManager::RecomputeArenaHighWater()
{
    arenaHighWater = 0;
    for (const auto& [id, state] : states) {
        (void)id;
        arenaHighWater = std::max(arenaHighWater, state.offset + state.capacity);
    }
}

void ParticleManager::DestroyState(EmitterState& state)
{
    ClearArenaRange(state.offset, state.capacity, kUnowned);
    const std::uint32_t offset = state.offset;
    const std::uint32_t capacity = state.capacity;
    state.capacity = 0;
    ReleaseRange(offset, capacity);
    ReleaseDescriptorSlot(state.descriptorSlot);
}

void ParticleManager::ResetAll()
{
    states.clear();
    queuedEmitters.clear();
    descriptorUpload.clear();
    freeRanges.clear();
    if (arenaCapacity > 0) {
        freeRanges.push_back({ 0, arenaCapacity });
        glad_glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSSBO);
        const GLuint zero = 0;
        glad_glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER,
                               GL_UNSIGNED_INT, &zero);
        glad_glBindBuffer(GL_SHADER_STORAGE_BUFFER, ownerSSBO);
        glad_glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER,
                               GL_UNSIGNED_INT, &kUnowned);
        glad_glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }
    freeDescriptorSlots.clear();
    descriptorHighWater = 0;
    arenaHighWater = 0;
    lastBatchedFrame = ~0ull;
}

ParticleManager::EmitterState& ParticleManager::GetOrCreate(ParticleComponent* emitter)
{
    const std::string& id = emitter->GetID();
    auto [it, inserted] = states.try_emplace(id);
    EmitterState& state = it->second;
    const std::uint32_t wanted = static_cast<std::uint32_t>(
        std::max(1, emitter->GetMaxParticles()));

    if (inserted) {
        state.descriptorSlot = AcquireDescriptorSlot();
        state.offset = AcquireRange(wanted);
        state.capacity = wanted;
        state.seed = static_cast<std::uint32_t>(std::hash<std::string>{}(id)) | 1u;
        ClearArenaRange(state.offset, state.capacity, state.descriptorSlot);
    } else if (state.capacity != wanted) {
        ClearArenaRange(state.offset, state.capacity, kUnowned);
        const std::uint32_t oldOffset = state.offset;
        const std::uint32_t oldCapacity = state.capacity;
        state.capacity = 0;
        ReleaseRange(oldOffset, oldCapacity);

        state.offset = AcquireRange(wanted);
        state.capacity = wanted;
        state.head = 0;
        state.emitAccumulator = 0.0f;
        state.emitterTime = 0.0f;
        state.startBurstFired = false;
        ClearArenaRange(state.offset, state.capacity, state.descriptorSlot);
    }
    return state;
}

bool ParticleManager::BeginSimulationFrame(float dt, std::uint64_t frameId)
{
    if (!EnsureInitialized()) return false;
    if (pendingReset) {
        ResetAll();
        pendingReset = false;
    }
    if (lastBatchedFrame == frameId) return false;

    lastBatchedFrame = frameId;
    currentFrame = frameId;
    frameDt = dt;
    queuedEmitters.clear();
    return true;
}

void ParticleManager::QueueEmitter(ParticleComponent* emitter,
                                   const glm::vec2& emitterPos, float emitterRot)
{
    if (emitter->GetSimulationBackend() == ParticleComponent::SimulationBackend::CUDA &&
        !warnedCudaDeferred) {
        std::cerr << "ParticleManager: CUDA particle simulation is deferred during the "
                     "batched particle-world revamp; using GLSL compute." << std::endl;
        warnedCudaDeferred = true;
    }

    EmitterState& state = GetOrCreate(emitter);
    state.lastTouchedFrame = currentFrame;
    if (frameDt <= 0.0f) return;

    const float duration = emitter->GetDuration();
    state.emitterTime += frameDt;
    bool emittingContinuous = true;
    if (emitter->GetLooping()) {
        if (duration > 0.0f && state.emitterTime >= duration) {
            state.emitterTime = std::fmod(state.emitterTime, duration);
            state.startBurstFired = false;
        }
    } else if (duration > 0.0f && state.emitterTime > duration) {
        emittingContinuous = false;
    }

    int emitCount = 0;
    if (!state.startBurstFired) {
        emitCount += std::max(0, emitter->GetStartBurst());
        state.startBurstFired = true;
    }
    if (emittingContinuous) {
        state.emitAccumulator += emitter->GetEmissionRate() * frameDt;
        const int whole = static_cast<int>(std::floor(state.emitAccumulator));
        if (whole > 0) {
            state.emitAccumulator -= static_cast<float>(whole);
            emitCount += whole;
        }
    }
    emitCount += emitter->ConsumePendingBurst();
    emitCount = std::clamp(emitCount, 0, static_cast<int>(state.capacity));

    const glm::vec2 shapeSize = EngineUtils::RenderUtils::PixelsToWorld(emitter->GetShapeSize());
    const glm::vec2 speed = emitter->GetSpeed();
    const glm::vec2 lifetime = emitter->GetLifetime();
    const glm::vec2 gravity = emitter->GetGravity();

    ParticleEmitterGpuData descriptor{};
    descriptor.meta[0] = state.offset;
    descriptor.meta[1] = state.capacity;
    descriptor.meta[2] = state.head;
    descriptor.meta[3] = static_cast<std::uint32_t>(emitCount);
    descriptor.flags[0] = 1;
    descriptor.flags[1] = state.seed ^ static_cast<std::uint32_t>(currentFrame * 2654435761u);
    descriptor.flags[2] = static_cast<std::uint32_t>(emitter->GetSpace());
    descriptor.flags[3] = static_cast<std::uint32_t>(emitter->GetShape());
    descriptor.pose[0] = emitterPos.x;
    descriptor.pose[1] = emitterPos.y;
    descriptor.pose[2] = emitterRot;
    descriptor.pose[3] = emitter->GetConeAngle();
    descriptor.shapeMotion[0] = shapeSize.x;
    descriptor.shapeMotion[1] = shapeSize.y;
    descriptor.shapeMotion[2] = emitter->GetDirection();
    descriptor.shapeMotion[3] = emitter->GetSpread();
    descriptor.speedLifetime[0] = speed.x;
    descriptor.speedLifetime[1] = speed.y;
    descriptor.speedLifetime[2] = lifetime.x;
    descriptor.speedLifetime[3] = lifetime.y;
    descriptor.physics[0] = gravity.x;
    descriptor.physics[1] = gravity.y;
    descriptor.physics[2] = emitter->GetDamping();
    descriptor.physics[3] = frameDt;

    queuedEmitters.push_back({ &state, descriptor });
    if (emitCount > 0)
        state.head = (state.head + static_cast<std::uint32_t>(emitCount)) % state.capacity;
}

void ParticleManager::EndSimulationFrame()
{
    if (queuedEmitters.empty() || arenaHighWater == 0) return;

    ROCK_PROFILE_COUNT("Particle emitters", queuedEmitters.size());
    ROCK_PROFILE_COUNT("Particle arena slots", arenaHighWater);
    ROCK_PROFILE_COUNT("Particle compute dispatches", 1);

    descriptorUpload.assign(descriptorHighWater, ParticleEmitterGpuData{});
    for (const QueuedEmitter& queued : queuedEmitters)
        descriptorUpload[queued.state->descriptorSlot] = queued.descriptor;

    glad_glBindBuffer(GL_SHADER_STORAGE_BUFFER, emitterSSBO);
    glad_glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
        static_cast<GLsizeiptr>(descriptorUpload.size()) * sizeof(ParticleEmitterGpuData),
        descriptorUpload.data());

    glad_glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleSSBO);
    glad_glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ownerSSBO);
    glad_glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, emitterSSBO);
    glad_glUseProgram(simProgram);
    SetU1ui(simProgram, "uParticleHighWater", arenaHighWater);

    glad_glDispatchCompute((arenaHighWater + kLocalSize - 1) / kLocalSize, 1, 1);
    glad_glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glad_glUseProgram(0);
    glad_glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    queuedEmitters.clear();
}

void ParticleManager::Draw(ParticleComponent* emitter, const glm::mat4& view,
                           const glm::mat4& proj, const glm::mat4& model,
                           unsigned int textureId, const glm::vec2& uvScale,
                           const glm::vec2& uvOffset, unsigned int vao)
{
    auto it = states.find(emitter->GetID());
    if (it == states.end() || it->second.capacity == 0) return;
    const EmitterState& state = it->second;

    glad_glUseProgram(drawProgram);
    glad_glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleSSBO);
    SetUMat4(drawProgram, "uView", view);
    SetUMat4(drawProgram, "uProj", proj);
    SetUMat4(drawProgram, "uModel", model);
    SetU4f(drawProgram, "uStartColor", emitter->GetStartColor());
    SetU4f(drawProgram, "uEndColor", emitter->GetEndColor());
    SetU1f(drawProgram, "uStartSize",
           EngineUtils::RenderUtils::PixelsToWorld(emitter->GetStartSize()));
    SetU1f(drawProgram, "uEndSize",
           EngineUtils::RenderUtils::PixelsToWorld(emitter->GetEndSize()));
    SetU2f(drawProgram, "uUVScale", uvScale);
    SetU2f(drawProgram, "uUVOffset", uvOffset);

    if (textureId != 0) {
        glad_glActiveTexture(GL_TEXTURE0);
        glad_glBindTexture(GL_TEXTURE_2D, textureId);
        SetU1i(drawProgram, "uTexture", 0);
        SetU1i(drawProgram, "uHasTexture", 1);
    } else {
        SetU1i(drawProgram, "uHasTexture", 0);
    }

    glad_glBindVertexArray(vao);
    glad_glDrawArraysInstancedBaseInstance(GL_TRIANGLES, 0, 6,
                                           static_cast<GLsizei>(state.capacity),
                                           state.offset);
    glad_glBindVertexArray(0);
    glad_glUseProgram(0);
}

void ParticleManager::GarbageCollect(std::uint64_t frameId)
{
    constexpr std::uint64_t kGraceFrames = 4;
    for (auto it = states.begin(); it != states.end();) {
        if (frameId > it->second.lastTouchedFrame + kGraceFrames) {
            DestroyState(it->second);
            it = states.erase(it);
        } else {
            ++it;
        }
    }
    RecomputeArenaHighWater();
}
