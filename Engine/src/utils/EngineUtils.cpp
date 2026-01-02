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
} 
