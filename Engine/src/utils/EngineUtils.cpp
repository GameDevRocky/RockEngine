#include "engine/utils/EngineUtils.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <random>
#include <filesystem>

#if defined(_WIN32)
    #include <windows.h>
#elif defined(__APPLE__)
    #include <mach-o/dyld.h>
    #include <climits>
#else
    #include <unistd.h>
    #include <climits>
#endif

namespace EngineUtils {

    std::string ExecutableDir() {
        namespace fs = std::filesystem;
#if defined(_WIN32)
        wchar_t buf[MAX_PATH];
        DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
        if (n == 0) return ".";
        fs::path p(std::wstring(buf, n));
#elif defined(__APPLE__)
        char buf[PATH_MAX];
        uint32_t size = sizeof(buf);
        if (_NSGetExecutablePath(buf, &size) != 0) return ".";
        fs::path p(buf);
#else
        char buf[PATH_MAX];
        ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
        if (n <= 0) return ".";
        buf[n] = '\0';
        fs::path p(buf);
#endif
        return p.parent_path().string();
    }

    std::string GetAssetPath(const std::string& relativePath) {
        // Resolve the asset root once: prefer the folder next to the executable when it
        // looks like a bundled build (Domain/ sits beside the binary); otherwise use the
        // compiled-in source root for local development.
        static const std::string root = [] {
            namespace fs = std::filesystem;
            std::error_code ec;
            const std::string exeDir = ExecutableDir();
            if (fs::exists(fs::path(exeDir) / "Domain", ec)) {
                return exeDir;
            }
            return std::string(PROJECT_ROOT);
        }();
        return root + "/" + relativePath;
    }

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
