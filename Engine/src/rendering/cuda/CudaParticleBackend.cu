#include "engine/rendering/core/CudaParticleBackend.hpp"

#include <glad/glad.h>
#include <cuda_gl_interop.h>
#include <cuda_runtime.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <sstream>

namespace {

bool g_initialized = false;
int g_device = -1;

std::string CurrentOpenGLDevice() {
    const auto* vendor = reinterpret_cast<const char*>(glad_glGetString(GL_VENDOR));
    const auto* renderer = reinterpret_cast<const char*>(glad_glGetString(GL_RENDERER));
    std::ostringstream out;
    out << "GL_VENDOR=" << (vendor ? vendor : "unknown")
        << ", GL_RENDERER=" << (renderer ? renderer : "unknown");
    return out.str();
}

std::string CudaError(const char* operation, cudaError_t result) {
    std::ostringstream out;
    out << operation << " failed: " << cudaGetErrorName(result)
        << " (" << cudaGetErrorString(result) << ")";
    return out.str();
}

__device__ std::uint32_t Hash(std::uint32_t value) {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

__device__ float Random(std::uint32_t& state) {
    state = Hash(state);
    return static_cast<float>(state) * (1.0f / 4294967296.0f);
}

__device__ void Rotate(float x, float y, float degrees, float& outX, float& outY) {
    constexpr float kDegreesToRadians = 0.01745329251994329577f;
    float sine = 0.0f;
    float cosine = 0.0f;
    sincosf(degrees * kDegreesToRadians, &sine, &cosine);
    outX = cosine * x - sine * y;
    outY = sine * x + cosine * y;
}

__global__ void EmitParticles(ParticleGpuData* particles, ParticleSimulationStep step) {
    const std::uint32_t gid = blockIdx.x * blockDim.x + threadIdx.x;
    if (gid >= step.emitCount) return;

    const std::uint32_t slot = (step.head + gid) % step.capacity;
    std::uint32_t randomState = Hash(step.frameSeed + gid * 747796405u + slot * 2891336453u);

    float offsetX = 0.0f;
    float offsetY = 0.0f;
    float direction = step.direction;

    if (step.shape == 1) {
        constexpr float kTwoPi = 6.28318530717958647692f;
        const float angle = Random(randomState) * kTwoPi;
        const float radius = sqrtf(Random(randomState));
        float sine = 0.0f;
        float cosine = 0.0f;
        sincosf(angle, &sine, &cosine);
        offsetX = cosine * radius * step.shapeSize[0];
        offsetY = sine * radius * step.shapeSize[1];
    } else if (step.shape == 2) {
        offsetX = (Random(randomState) * 2.0f - 1.0f) * step.shapeSize[0];
        offsetY = (Random(randomState) * 2.0f - 1.0f) * step.shapeSize[1];
    } else if (step.shape == 3) {
        direction += (Random(randomState) * 2.0f - 1.0f) * step.coneAngle * 0.5f;
    }

    const float speed = step.speedRange[0] +
        (step.speedRange[1] - step.speedRange[0]) * Random(randomState);
    const float lifetime = step.lifetimeRange[0] +
        (step.lifetimeRange[1] - step.lifetimeRange[0]) * Random(randomState);
    const float spread = (Random(randomState) * 2.0f - 1.0f) * step.spread * 0.5f;

    float velocityX = 0.0f;
    float velocityY = 0.0f;
    Rotate(speed, 0.0f, direction + spread, velocityX, velocityY);

    float positionX = offsetX;
    float positionY = offsetY;
    if (step.space == 1) {
        Rotate(offsetX, offsetY, step.emitterRotation, positionX, positionY);
        positionX += step.emitterPosition[0];
        positionY += step.emitterPosition[1];

        float worldVelocityX = 0.0f;
        float worldVelocityY = 0.0f;
        Rotate(velocityX, velocityY, step.emitterRotation, worldVelocityX, worldVelocityY);
        velocityX = worldVelocityX;
        velocityY = worldVelocityY;
    }

    ParticleGpuData particle{};
    particle.position[0] = positionX;
    particle.position[1] = positionY;
    particle.velocity[0] = velocityX;
    particle.velocity[1] = velocityY;
    particle.age = 0.0f;
    particle.lifetime = fmaxf(lifetime, 0.0001f);
    particle.seed = static_cast<float>(slot);
    particles[slot] = particle;
}

__global__ void IntegrateParticles(ParticleGpuData* particles, ParticleSimulationStep step) {
    const std::uint32_t gid = blockIdx.x * blockDim.x + threadIdx.x;
    if (gid >= step.capacity) return;

    ParticleGpuData particle = particles[gid];
    if (particle.age >= particle.lifetime) return;

    particle.velocity[0] += step.gravity[0] * step.dt;
    particle.velocity[1] += step.gravity[1] * step.dt;
    const float drag = expf(-step.damping * step.dt);
    particle.velocity[0] *= drag;
    particle.velocity[1] *= drag;
    particle.position[0] += particle.velocity[0] * step.dt;
    particle.position[1] += particle.velocity[1] * step.dt;
    particle.age += step.dt;
    particles[gid] = particle;
}

} // namespace

namespace CudaParticleBackend {

bool IsCompiled() { return true; }

bool EnsureInitialized(std::string& error) {
    if (g_initialized) return true;

    const auto* glVendor = reinterpret_cast<const char*>(glad_glGetString(GL_VENDOR));
    if (!glVendor || std::strstr(glVendor, "NVIDIA") == nullptr) {
        error = "the current OpenGL context is not running on an NVIDIA GPU (" +
            CurrentOpenGLDevice() + "). CUDA/OpenGL interop requires both APIs on "
            "the same NVIDIA adapter";
#if defined(_WIN32)
        error += ". Set Windows Settings > System > Display > Graphics > "
                 "the RockEngine executable to High performance, then restart it";
#endif
        return false;
    }

    unsigned int count = 0;
    int device = -1;
    cudaError_t result = cudaGLGetDevices(&count, &device, 1, cudaGLDeviceListCurrentFrame);
    if (result != cudaSuccess) {
        error = CudaError("cudaGLGetDevices", result) + " (" + CurrentOpenGLDevice() + ")";
        return false;
    }
    if (count == 0 || device < 0) {
        error = "the current OpenGL context is not associated with a CUDA device";
        return false;
    }

    result = cudaSetDevice(device);
    if (result != cudaSuccess) {
        error = CudaError("cudaSetDevice", result);
        return false;
    }

    g_device = device;
    g_initialized = true;
    return true;
}

bool RegisterBuffer(unsigned int glBuffer, ResourceHandle& resource, std::string& error) {
    if (resource) return true;
    if (!EnsureInitialized(error)) return false;

    cudaGraphicsResource* registered = nullptr;
    const cudaError_t result = cudaGraphicsGLRegisterBuffer(
        &registered, glBuffer, cudaGraphicsMapFlagsNone);
    if (result != cudaSuccess) {
        error = CudaError("cudaGraphicsGLRegisterBuffer", result);
        return false;
    }
    resource = registered;
    return true;
}

void UnregisterBuffer(ResourceHandle& resource) {
    if (!resource) return;
    cudaGraphicsUnregisterResource(static_cast<cudaGraphicsResource*>(resource));
    resource = nullptr;
}

bool Simulate(ResourceHandle resource, const ParticleSimulationStep& step, std::string& error) {
    if (!resource) {
        error = "CUDA particle buffer was not registered";
        return false;
    }
    if (g_device >= 0) {
        const cudaError_t setDeviceResult = cudaSetDevice(g_device);
        if (setDeviceResult != cudaSuccess) {
            error = CudaError("cudaSetDevice", setDeviceResult);
            return false;
        }
    }

    auto* registered = static_cast<cudaGraphicsResource*>(resource);
    cudaError_t result = cudaGraphicsMapResources(1, &registered, nullptr);
    if (result != cudaSuccess) {
        error = CudaError("cudaGraphicsMapResources", result);
        return false;
    }

    ParticleGpuData* particles = nullptr;
    std::size_t mappedBytes = 0;
    result = cudaGraphicsResourceGetMappedPointer(
        reinterpret_cast<void**>(&particles), &mappedBytes, registered);
    if (result != cudaSuccess) {
        error = CudaError("cudaGraphicsResourceGetMappedPointer", result);
    } else if (mappedBytes <
        static_cast<std::size_t>(step.capacity) * sizeof(ParticleGpuData)) {
        error = "mapped CUDA particle buffer is smaller than its declared capacity";
        result = cudaErrorInvalidValue;
    }

    constexpr std::uint32_t kBlockSize = 64;
    if (result == cudaSuccess && step.emitCount > 0) {
        const std::uint32_t blocks = (step.emitCount + kBlockSize - 1) / kBlockSize;
        EmitParticles<<<blocks, kBlockSize>>>(particles, step);
        result = cudaGetLastError();
        if (result != cudaSuccess) error = CudaError("CUDA particle emission kernel", result);
    }
    if (result == cudaSuccess) {
        const std::uint32_t blocks = (step.capacity + kBlockSize - 1) / kBlockSize;
        IntegrateParticles<<<blocks, kBlockSize>>>(particles, step);
        result = cudaGetLastError();
        if (result != cudaSuccess) error = CudaError("CUDA particle integration kernel", result);
    }

    const cudaError_t unmapResult = cudaGraphicsUnmapResources(1, &registered, nullptr);
    if (result == cudaSuccess && unmapResult != cudaSuccess) {
        error = CudaError("cudaGraphicsUnmapResources", unmapResult);
        result = unmapResult;
    }
    return result == cudaSuccess;
}

void Shutdown() {
    if (g_initialized) cudaDeviceSynchronize();
    g_initialized = false;
    g_device = -1;
}

} // namespace CudaParticleBackend
