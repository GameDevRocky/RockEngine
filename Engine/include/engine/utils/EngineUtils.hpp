
#pragma once
#include <atomic>
#include <string>
#include <random>
#include <sstream>
#include <string_view>
#include <glm/glm.hpp>

namespace EngineUtils{

    enum EventType{
        UNDEFINED,
        NAME_CHANGED,


    };

    namespace RenderUtils{

        constexpr float PixelsPerUnit = 32.0f;
        
        inline float PixelsToWorld(float pixels) {
            return pixels / PixelsPerUnit;
        }
        
        inline glm::vec2 PixelsToWorld(const glm::vec2& pixels) {
            return pixels / PixelsPerUnit;
        }
        
        inline float WorldToPixels(float units) {
            return units * PixelsPerUnit;
        }
        
        inline glm::vec2 WorldToPixels(const glm::vec2& units) {
            return units * PixelsPerUnit;
        }
    
    }

    namespace MathUtils{
        constexpr float RAD_2_DEG = 180.0f / 3.14159265359f;
        constexpr float DEG_2_RAD = 3.14159265359f / 180.0f;
        constexpr float PI = 3.14159265359f;


    }





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