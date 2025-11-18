
#pragma once
#include <atomic>
#include <string>
#include <random>
#include <sstream>
namespace EngineUtils{


std::string GenerateUUID();
std::string ReadShader(const std::string& path);
}