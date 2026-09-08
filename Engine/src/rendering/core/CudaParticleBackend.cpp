#include "engine/rendering/core/CudaParticleBackend.hpp"

#ifndef ROCKENGINE_HAS_CUDA

namespace CudaParticleBackend {

bool IsCompiled() { return false; }

bool EnsureInitialized(std::string& error) {
    error = "this RockEngine build was compiled without CUDA support";
    return false;
}

bool RegisterBuffer(unsigned int, ResourceHandle& resource, std::string& error) {
    resource = nullptr;
    error = "this RockEngine build was compiled without CUDA support";
    return false;
}

void UnregisterBuffer(ResourceHandle& resource) { resource = nullptr; }

bool Simulate(ResourceHandle, const ParticleSimulationStep&, std::string& error) {
    error = "this RockEngine build was compiled without CUDA support";
    return false;
}

void Shutdown() {}

} // namespace CudaParticleBackend

#endif

