#pragma once

#include <string>

#include "engine/rendering/core/ParticleSimulationTypes.hpp"

// CUDA is deliberately hidden behind this plain C++ boundary. In non-CUDA
// builds CudaParticleBackend.cpp supplies a stub, so no CUDA header or type
// leaks into RockEngineCore's public interface.
namespace CudaParticleBackend {

using ResourceHandle = void*;

bool IsCompiled();
bool EnsureInitialized(std::string& error);
bool RegisterBuffer(unsigned int glBuffer, ResourceHandle& resource, std::string& error);
void UnregisterBuffer(ResourceHandle& resource);
bool Simulate(ResourceHandle resource, const ParticleSimulationStep& step, std::string& error);
void Shutdown();

} // namespace CudaParticleBackend

