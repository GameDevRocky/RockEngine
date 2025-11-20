
#pragma once
#include <atomic>
#include <string>
#include <random>
#include <sstream>
#include <string_view>

namespace EngineUtils{


std::string GenerateUUID();
std::string ReadShader(const std::string& path);

template<typename T>
constexpr std::string_view TypeName() {
    std::string_view n = __PRETTY_FUNCTION__;

#if defined(__clang__) || defined(__GNUC__)
   
    auto start = n.find("T = ") + 4;
    auto end = n.find(';', start);

    return n.substr(start, end - start);

#elif defined(_MSC_VER)
    auto start = n.find('<') + 1;
    auto end = n.find('>', start);
    return n.substr(start, end - start);
#endif
}

}