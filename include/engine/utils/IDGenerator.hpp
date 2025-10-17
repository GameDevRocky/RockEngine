
#pragma once
#include <atomic>
#include <string>
#include <random>
#include <sstream>

class IDGenerator {
public:
    static std::string GenerateUUID() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);

        const char* hex_chars = "0123456789abcdef";
        std::stringstream ss;

        for (int i = 0; i < 32; ++i)
            ss << hex_chars[dis(gen)];

        return ss.str();
    }
};
