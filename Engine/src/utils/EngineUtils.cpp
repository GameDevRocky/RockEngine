#include "engine/utils/EngineUtils.hpp"
#include "engine/jobs/MainThread.hpp"
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

    std::string GetAssetRoot() {
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
        return root;
    }

    std::string GetAssetPath(const std::string& relativePath) {
        return GetAssetRoot() + "/" + relativePath;
    }

    std::string ToAssetRelative(const std::string& absPath) {
        namespace fs = std::filesystem;
        std::error_code ec;
        fs::path rel = fs::relative(fs::path(absPath), fs::path(GetAssetRoot()), ec);
        if (ec || rel.empty()) return absPath;
        return rel.generic_string();
    }

    std::string GenerateUUID() {
        // Every Serializable constructor lands here, so this is the single most
        // likely way for a worker thread to corrupt shared state: `gen` and
        // `dis` are shared and mutated on every call. Thread-safe to
        // *initialize*, not to *use*. A job's worker half must never construct
        // an engine object -- this is what catches it if one tries.
        ROCK_ASSERT_MAIN_THREAD();

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
        ShaderDomain domain = ShaderDomain::Sprite;

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
            // Read only in the preamble, before any stage marker. Everything up
            // there was already being discarded, so recognising a directive here
            // cannot change how any existing shader compiles.
            if (current == Section::None &&
                line.find("#pragma domain") != std::string::npos) {
                if (line.find("text") != std::string::npos) domain = ShaderDomain::Text;
                continue;
            }
            if      (current == Section::Vertex)   vertSS << line << '\n';
            else if (current == Section::Fragment)  fragSS << line << '\n';
        }

        return { vertSS.str(), fragSS.str(), domain };
    }
} 
