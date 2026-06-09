#include "engine/utils/EngineUtils.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <random>

namespace EngineUtils {

    std::string GenerateUUID() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);

        const char* hex_chars = "0123456789abcdef";
        std::stringstream ss;

        for (int i = 0; i < 32; ++i)
            ss << hex_chars[dis(gen)];

        return ss.str();
    }

    std::string ReadShader(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "Failed to open shader file: " << path << std::endl;
            return "";
        }
        std::stringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

    ShaderSource ParseShaderSource(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "Failed to open shader source: " << path << std::endl;
            return {};
        }

        enum class Section { None, Vertex, Fragment };
        Section current = Section::None;
        std::stringstream vertSS, fragSS;

        std::string line;
        while (std::getline(file, line)) {
            if (line.find("#pragma vertex") != std::string::npos) {
                current = Section::Vertex;
                continue;
            }
            if (line.find("#pragma fragment") != std::string::npos) {
                current = Section::Fragment;
                continue;
            }
            if      (current == Section::Vertex)   vertSS << line << '\n';
            else if (current == Section::Fragment)  fragSS << line << '\n';
        }

        return { vertSS.str(), fragSS.str() };
    }
} 
