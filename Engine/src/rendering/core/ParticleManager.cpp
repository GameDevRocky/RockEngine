#include "engine/rendering/core/ParticleManager.hpp"
#include "engine/components/ParticleComponent.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "Engine.hpp"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

// A particle is 8 floats / 32 bytes, std430-friendly (vec2s first). Must match
// the `struct Particle` declared in every shader below.
static constexpr int kParticleFloats = 8;
static constexpr int kParticleBytes  = kParticleFloats * sizeof(float);
static constexpr int kLocalSize      = 64;

// ── Shared GLSL: the particle struct + SSBO binding, prepended to each stage ──
static const char* kParticleStructGLSL = R"(
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

static const char* kEmitComputeMain = R"(
uniform uint  uEmitCount;
uniform uint  uCapacity;
uniform uint  uHead;
uniform uint  uFrameSeed;
uniform int   uSpace;        // 0=Local, 1=World
uniform vec2  uEmitterPos;
uniform float uEmitterRot;   // degrees
uniform int   uShape;        // 0=Point,1=Circle,2=Box,3=Cone
uniform vec2  uShapeSize;
uniform float uConeAngle;    // degrees
uniform float uDirection;    // degrees, from +X
uniform float uSpread;       // degrees
uniform vec2  uSpeedRange;   // min, max
uniform vec2  uLifeRange;    // min, max

const float PI = 3.14159265;
uint hash(uint x){ x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16; return x; }
float rnd(inout uint s){ s = hash(s); return float(s) * (1.0 / 4294967296.0); }
vec2 rot(vec2 v, float deg){ float r = radians(deg); float c = cos(r), s = sin(r); return vec2(c*v.x - s*v.y, s*v.x + c*v.y); }

void main(){
    uint gid = gl_GlobalInvocationID.x;
    if (gid >= uEmitCount) return;
    uint slot = (uHead + gid) % uCapacity;

    uint s = hash(uFrameSeed + gid * 747796405u + slot * 2891336453u);

    vec2 off = vec2(0.0);
    float dirDeg = uDirection;
    if (uShape == 1) {            // Circle (filled disk)
        float a = rnd(s) * 2.0 * PI;
        float rr = sqrt(rnd(s));
        off = vec2(cos(a), sin(a)) * rr * uShapeSize;
    } else if (uShape == 2) {     // Box
        off = (vec2(rnd(s), rnd(s)) * 2.0 - 1.0) * uShapeSize;
    } else if (uShape == 3) {     // Cone: bias direction within the cone half-angle
        dirDeg = uDirection + (rnd(s) * 2.0 - 1.0) * uConeAngle * 0.5;
    }

    float spd  = mix(uSpeedRange.x, uSpeedRange.y, rnd(s));
    float life = mix(uLifeRange.x,  uLifeRange.y,  rnd(s));
    float spreadDeg = (rnd(s) * 2.0 - 1.0) * uSpread * 0.5;

    vec2 pos = off;
    vec2 vel = rot(vec2(1.0, 0.0), dirDeg + spreadDeg) * spd;

    if (uSpace == 1) {            // World: bake emitter pose into spawn state
        pos = uEmitterPos + rot(off, uEmitterRot);
        vel = rot(vel, uEmitterRot);
    }

    Particle p;
    p.position = pos;
    p.velocity = vel;
    p.age = 0.0;
    p.lifetime = max(life, 0.0001);
    p.seed = float(slot);
    p._pad = 0.0;
    particles[slot] = p;
}
)";

static const char* kSimComputeMain = R"(
uniform uint  uCapacity;
uniform float uDt;
uniform vec2  uGravity;
uniform float uDamping;

void main(){
    uint gid = gl_GlobalInvocationID.x;
    if (gid >= uCapacity) return;
    Particle p = particles[gid];
    if (p.age >= p.lifetime) return;   // dead slot (also true for zero-inited buffer)
    p.velocity += uGravity * uDt;
    p.velocity *= exp(-uDamping * uDt);
    p.position += p.velocity * uDt;
    p.age += uDt;
    particles[gid] = p;
}
)";

static const char* kDrawVert = R"(
layout(location = 0) in vec2 aPos;   // unit quad in [-0.5, 0.5]
layout(location = 1) in vec2 aUV;
uniform mat4 uView;
uniform mat4 uProj;
uniform mat4 uModel;                 // identity (World) or emitter world matrix (Local)
uniform vec4 uStartColor;
uniform vec4 uEndColor;
uniform float uStartSize;
uniform float uEndSize;
out vec2 vUV;
out vec4 vColor;
void main(){
    Particle p = particles[gl_InstanceID];
    if (p.lifetime <= 0.0 || p.age >= p.lifetime) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);   // outside clip volume -> culled
        vUV = aUV; vColor = vec4(0.0);
        return;
    }
    float t = clamp(p.age / p.lifetime, 0.0, 1.0);
    float size = mix(uStartSize, uEndSize, t);
    vColor = mix(uStartColor, uEndColor, t);
    vec2 local = p.position + aPos * size;
    gl_Position = uProj * uView * uModel * vec4(local, 0.0, 1.0);
    vUV = aUV;
}
)";

static const char* kDrawFrag = R"(#version 460 core
in vec2 vUV;
in vec4 vColor;
uniform sampler2D uTexture;
uniform int uHasTexture;
out vec4 FragColor;
void main(){
    vec4 tex = (uHasTexture == 1) ? texture(uTexture, vUV) : vec4(1.0);
    FragColor = tex * vColor;
    if (FragColor.a <= 0.001) discard;
}
)";

// ── Compile helpers (inline, PickingPass style -- the Shader asset class only
//    supports vertex+fragment, so compute is compiled here directly). ─────────
static GLuint CompileStage(GLenum type, const std::vector<const char*>& srcs)
{
    GLuint sh = glad_glCreateShader(type);
    glad_glShaderSource(sh, static_cast<GLsizei>(srcs.size()), srcs.data(), nullptr);
    glad_glCompileShader(sh);
    GLint ok = 0;
    glad_glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glad_glGetShaderInfoLog(sh, sizeof(log), nullptr, log);
        std::cerr << "ParticleManager: shader compile failed:\n" << log << std::endl;
    }
    return sh;
}

static GLuint LinkProgram(const std::vector<GLuint>& stages)
{
    GLuint prog = glad_glCreateProgram();
    for (GLuint s : stages) glad_glAttachShader(prog, s);
    glad_glLinkProgram(prog);
    GLint ok = 0;
    glad_glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glad_glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        std::cerr << "ParticleManager: program link failed:\n" << log << std::endl;
    }
    for (GLuint s : stages) glad_glDeleteShader(s);
    return prog;
}

static GLuint BuildCompute(const char* mainSrc)
{
    const char* version   = "#version 460 core\n";
    const char* localSize = "layout(local_size_x = 64) in;\n";
    GLuint cs = CompileStage(GL_COMPUTE_SHADER, { version, localSize, kParticleStructGLSL, mainSrc });
    return LinkProgram({ cs });
}

ParticleManager& ParticleManager::Get()
{
    static ParticleManager instance;
    return instance;
}

bool ParticleManager::EnsureInitialized()
{
    if (initialized) return true;

    emitProgram = BuildCompute(kEmitComputeMain);
    simProgram  = BuildCompute(kSimComputeMain);

    // Draw program: the particle struct/SSBO is only needed in the vertex stage.
    const char* version = "#version 460 core\n";
    GLuint vs = CompileStage(GL_VERTEX_SHADER, { version, kParticleStructGLSL, kDrawVert });
    GLuint fs = CompileStage(GL_FRAGMENT_SHADER, { kDrawFrag });
    drawProgram = LinkProgram({ vs, fs });

    // Shared unit quad (two triangles), pos + uv -- same layout as ScenePass.
    float quad[] = {
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

    // Reset all emitter GPU state on play-mode enter/exit so a run starts fresh
    // and returning to the editor starts fresh (editor + runtime emitters share
    // an id). Deletion needs a current context, which these events don't
    // guarantee, so just flag it -- Simulate() services it (context-current).
    Engine::Get()->Subscribe([this]() { pendingReset = true; return true; },
                             Engine::ENTER_PLAY_MODE_EVENT);
    Engine::Get()->Subscribe([this]() { pendingReset = true; return true; },
                             Engine::EXIT_PLAY_MODE_EVENT);

    initialized = true;
    return true;
}

void ParticleManager::DestroyState(EmitterState& st)
{
    if (st.particleSSBO) glad_glDeleteBuffers(1, &st.particleSSBO);
    st.particleSSBO = 0;
    st.capacity = 0;
}

void ParticleManager::ResetAll()
{
    for (auto& [id, st] : states) DestroyState(st);
    states.clear();
}

ParticleManager::EmitterState& ParticleManager::GetOrCreate(ParticleComponent* emitter)
{
    const std::string& id = emitter->GetID();
    EmitterState& st = states[id];

    const int wanted = std::max(1, emitter->GetMaxParticles());
    if (st.particleSSBO != 0 && st.capacity == wanted)
        return st;

    // (Re)allocate: capacity changed or first sight of this emitter.
    if (st.particleSSBO) glad_glDeleteBuffers(1, &st.particleSSBO);

    st.capacity = wanted;
    st.head = 0;
    st.emitAccumulator = 0.0f;
    st.emitterTime = 0.0f;
    st.startBurstFired = false;
    st.seed = static_cast<std::uint32_t>(std::hash<std::string>{}(id)) | 1u;

    glad_glGenBuffers(1, &st.particleSSBO);
    glad_glBindBuffer(GL_SHADER_STORAGE_BUFFER, st.particleSSBO);
    glad_glBufferData(GL_SHADER_STORAGE_BUFFER,
                      static_cast<GLsizeiptr>(st.capacity) * kParticleBytes,
                      nullptr, GL_DYNAMIC_DRAW);
    // Zero-init => every slot has age==lifetime==0, i.e. dead (culled at draw).
    GLuint zero = 0;
    glad_glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER,
                           GL_UNSIGNED_INT, &zero);
    glad_glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    return st;
}

static void SetU1i(GLuint p, const char* n, int v)   { glad_glUniform1i(glad_glGetUniformLocation(p, n), v); }
static void SetU1ui(GLuint p, const char* n, GLuint v){ glad_glUniform1ui(glad_glGetUniformLocation(p, n), v); }
static void SetU1f(GLuint p, const char* n, float v)  { glad_glUniform1f(glad_glGetUniformLocation(p, n), v); }
static void SetU2f(GLuint p, const char* n, const glm::vec2& v){ glad_glUniform2f(glad_glGetUniformLocation(p, n), v.x, v.y); }
static void SetU4f(GLuint p, const char* n, const glm::vec4& v){ glad_glUniform4f(glad_glGetUniformLocation(p, n), v.x, v.y, v.z, v.w); }
static void SetUMat4(GLuint p, const char* n, const glm::mat4& m){ glad_glUniformMatrix4fv(glad_glGetUniformLocation(p, n), 1, GL_FALSE, glm::value_ptr(m)); }

void ParticleManager::Simulate(ParticleComponent* emitter, const glm::vec2& emitterPos,
                               float emitterRot, float dt, std::uint64_t frameId)
{
    if (pendingReset) { ResetAll(); pendingReset = false; }

    EmitterState& st = GetOrCreate(emitter);

    // Once per frame regardless of how many viewports draw it. Still mark touched
    // so GC keeps the buffer alive for the second view's draw.
    if (st.lastSimulatedFrame == frameId) { st.lastTouchedFrame = frameId; return; }
    st.lastSimulatedFrame = frameId;
    st.lastTouchedFrame = frameId;

    if (dt <= 0.0f) return;

    const int capacity = st.capacity;
    const float duration = emitter->GetDuration();

    // Advance the emitter timeline (drives duration / looping / loop bursts).
    st.emitterTime += dt;
    bool emittingContinuous = true;
    if (emitter->GetLooping()) {
        if (duration > 0.0f && st.emitterTime >= duration) {
            st.emitterTime = std::fmod(st.emitterTime, duration);
            st.startBurstFired = false;   // refire the start burst each loop
        }
    } else if (duration > 0.0f && st.emitterTime > duration) {
        emittingContinuous = false;
    }

    int emit = 0;
    if (!st.startBurstFired) { emit += std::max(0, emitter->GetStartBurst()); st.startBurstFired = true; }
    if (emittingContinuous) {
        st.emitAccumulator += emitter->GetEmissionRate() * dt;
        int whole = static_cast<int>(std::floor(st.emitAccumulator));
        if (whole > 0) { st.emitAccumulator -= static_cast<float>(whole); emit += whole; }
    }
    emit += emitter->ConsumePendingBurst();
    emit = std::clamp(emit, 0, capacity);

    glad_glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, st.particleSSBO);

    // ── Emit pass ──
    if (emit > 0) {
        glad_glUseProgram(emitProgram);
        SetU1ui(emitProgram, "uEmitCount", static_cast<GLuint>(emit));
        SetU1ui(emitProgram, "uCapacity",  static_cast<GLuint>(capacity));
        SetU1ui(emitProgram, "uHead",      st.head);
        SetU1ui(emitProgram, "uFrameSeed", st.seed ^ static_cast<GLuint>(frameId * 2654435761u));
        SetU1i (emitProgram, "uSpace",     static_cast<int>(emitter->GetSpace()));
        SetU2f (emitProgram, "uEmitterPos", emitterPos);
        SetU1f (emitProgram, "uEmitterRot", emitterRot);
        SetU1i (emitProgram, "uShape",     static_cast<int>(emitter->GetShape()));
        SetU2f (emitProgram, "uShapeSize", EngineUtils::RenderUtils::PixelsToWorld(emitter->GetShapeSize()));
        SetU1f (emitProgram, "uConeAngle", emitter->GetConeAngle());
        SetU1f (emitProgram, "uDirection", emitter->GetDirection());
        SetU1f (emitProgram, "uSpread",    emitter->GetSpread());
        SetU2f (emitProgram, "uSpeedRange", emitter->GetSpeed());
        SetU2f (emitProgram, "uLifeRange",  emitter->GetLifetime());

        GLuint groups = (static_cast<GLuint>(emit) + kLocalSize - 1) / kLocalSize;
        glad_glDispatchCompute(groups, 1, 1);
        glad_glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        st.head = (st.head + static_cast<GLuint>(emit)) % static_cast<GLuint>(capacity);
    }

    // ── Integrate pass ──
    glad_glUseProgram(simProgram);
    SetU1ui(simProgram, "uCapacity", static_cast<GLuint>(capacity));
    SetU1f (simProgram, "uDt",       dt);
    SetU2f (simProgram, "uGravity",  emitter->GetGravity());
    SetU1f (simProgram, "uDamping",  emitter->GetDamping());
    GLuint simGroups = (static_cast<GLuint>(capacity) + kLocalSize - 1) / kLocalSize;
    glad_glDispatchCompute(simGroups, 1, 1);
    glad_glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glad_glUseProgram(0);
}

void ParticleManager::Draw(ParticleComponent* emitter, const glm::mat4& view,
                           const glm::mat4& proj, const glm::mat4& model,
                           unsigned int textureId, unsigned int vao)
{
    auto it = states.find(emitter->GetID());
    if (it == states.end() || it->second.particleSSBO == 0) return;
    EmitterState& st = it->second;

    glad_glUseProgram(drawProgram);
    glad_glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, st.particleSSBO);

    SetUMat4(drawProgram, "uView",  view);
    SetUMat4(drawProgram, "uProj",  proj);
    SetUMat4(drawProgram, "uModel", model);
    SetU4f  (drawProgram, "uStartColor", emitter->GetStartColor());
    SetU4f  (drawProgram, "uEndColor",   emitter->GetEndColor());
    // Sizes are authored in PIXELS (like a sprite's pixel size). Cross to world
    // units via the engine's single pixel<->world conversion so particles match
    // sprite scale (a 24px particle is 24 world units, not 24px of a tiny quad).
    SetU1f  (drawProgram, "uStartSize",  EngineUtils::RenderUtils::PixelsToWorld(emitter->GetStartSize()));
    SetU1f  (drawProgram, "uEndSize",    EngineUtils::RenderUtils::PixelsToWorld(emitter->GetEndSize()));

    if (textureId != 0) {
        glad_glActiveTexture(GL_TEXTURE0);
        glad_glBindTexture(GL_TEXTURE_2D, textureId);
        SetU1i(drawProgram, "uTexture", 0);
        SetU1i(drawProgram, "uHasTexture", 1);
    } else {
        SetU1i(drawProgram, "uHasTexture", 0);
    }

    glad_glBindVertexArray(vao);
    glad_glDrawArraysInstanced(GL_TRIANGLES, 0, 6, st.capacity);
    glad_glBindVertexArray(0);
    glad_glUseProgram(0);
}

void ParticleManager::GarbageCollect(std::uint64_t frameId)
{
    // Free emitters not touched for a few frames (deleted / scene unloaded).
    // The small grace window tolerates a frame where no view drew them.
    constexpr std::uint64_t kGrace = 4;
    for (auto it = states.begin(); it != states.end();) {
        if (frameId > it->second.lastTouchedFrame + kGrace) {
            DestroyState(it->second);
            it = states.erase(it);
        } else {
            ++it;
        }
    }
}
