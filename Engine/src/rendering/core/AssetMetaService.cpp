#include "engine/rendering/core/AssetMetaService.hpp"
#include "engine/utils/EngineUtils.hpp"

#include <yaml-cpp/yaml.h>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

// ──────────────────────────────────────────────────────────────────────────────
bool AssetMetaService::IsMetaExtension(const std::string& ext) {
    return ext == ".texture" || ext == ".shader" || ext == ".material" || ext == ".font"
        || ext == ".audio";
}

std::string AssetMetaService::MetaExtensionFor(const std::string& sourceExt) {
    if (sourceExt == ".png"  || sourceExt == ".jpg"  ||
        sourceExt == ".jpeg" || sourceExt == ".bmp")   return ".texture";
    if (sourceExt == ".glsl")                           return ".shader";
    if (sourceExt == ".ttf"  || sourceExt == ".otf")    return ".font";
    if (sourceExt == ".wav"  || sourceExt == ".mp3"  ||
        sourceExt == ".ogg"  || sourceExt == ".flac")   return ".audio";
    return "";
}

// ──────────────────────────────────────────────────────────────────────────────
std::string AssetMetaService::ToProjectRelative(const fs::path& absPath) {
    fs::path root = fs::path(PROJECT_ROOT);
    std::error_code ec;
    fs::path rel = fs::relative(absPath, root, ec);
    if (ec) return absPath.string();

    // Normalise to forward slashes
    std::string s = rel.generic_string();
    return s;
}


void AssetMetaService::ProcessTexture(const fs::path& sourcePath) {
    fs::path metaPath = fs::path(sourcePath.string() + ".texture");
    if (fs::exists(metaPath)) return;  // already has meta

    std::string id       = EngineUtils::GenerateUUID();
    std::string name     = sourcePath.stem().string();
    std::string relPath  = ToProjectRelative(sourcePath);

    YAML::Node node;
    node["type"]      = "Texture2D";
    node["id"]        = id;
    node["name"]      = name;
    node["path"]      = relPath;
    node["filtering"] = "linear";
    node["wrap"]      = "repeat";
    node["sprites"]   = YAML::Node(YAML::NodeType::Sequence);

    std::ofstream out(metaPath);
    if (!out.is_open()) {
        std::cerr << "[AssetMetaService] Failed to write: " << metaPath << std::endl;
        return;
    }
    out << node;
    std::cout << "[AssetMetaService] Generated: " << metaPath.filename() << std::endl;
}

// ──────────────────────────────────────────────────────────────────────────────
void AssetMetaService::ProcessShader(const fs::path& glslPath) {
    fs::path metaPath = fs::path(glslPath.string() + ".shader");
    if (fs::exists(metaPath)) return;

    std::string id          = EngineUtils::GenerateUUID();
    std::string name        = glslPath.stem().string();
    std::string relSource   = ToProjectRelative(glslPath);

    YAML::Node node;
    node["type"]        = "Shader";
    node["id"]          = id;
    node["name"]        = name;
    node["source_path"] = relSource;

    std::ofstream out(metaPath);
    if (!out.is_open()) {
        std::cerr << "[AssetMetaService] Failed to write: " << metaPath << std::endl;
        return;
    }
    out << node;
    std::cout << "[AssetMetaService] Generated: " << metaPath.filename() << std::endl;
}

// ──────────────────────────────────────────────────────────────────────────────
void AssetMetaService::ProcessFont(const fs::path& fontPath) {
    fs::path metaPath = fs::path(fontPath.string() + ".font");
    if (fs::exists(metaPath)) return;

    std::string id      = EngineUtils::GenerateUUID();
    std::string name    = fontPath.stem().string();
    std::string relPath = ToProjectRelative(fontPath);

    YAML::Node node;
    node["type"] = "Font";
    node["id"]   = id;
    node["name"] = name;
    node["path"] = relPath;

    // The bake recipe. The atlas itself is never written to disk -- it is rebuilt
    // in memory from the source font on first use each session, so changing these
    // numbers takes effect on the next launch with nothing to invalidate.
    YAML::Node bake;
    bake["em_size"]  = 48;      // atlas pixels per em
    bake["px_range"] = 4.0f;    // distance range, atlas pixels
    node["bake"] = bake;
    // `charset` is deliberately omitted: absent means printable ASCII. Add it by
    // hand for a font that needs more (an icon font's glyphs sit far outside
    // ASCII and will bake empty until you do).

    std::ofstream out(metaPath);
    if (!out.is_open()) {
        std::cerr << "[AssetMetaService] Failed to write: " << metaPath << std::endl;
        return;
    }
    out << node;
    std::cout << "[AssetMetaService] Generated: " << metaPath.filename() << std::endl;
}

// ──────────────────────────────────────────────────────────────────────────────
void AssetMetaService::ProcessAudio(const fs::path& audioPath) {
    fs::path metaPath = fs::path(audioPath.string() + ".audio");
    if (fs::exists(metaPath)) return;  // already has meta

    std::string id       = EngineUtils::GenerateUUID();
    std::string name     = audioPath.stem().string();
    std::string relPath  = ToProjectRelative(audioPath);

    YAML::Node node;
    node["type"] = "AudioClip";
    node["id"]   = id;
    node["name"] = name;
    node["path"] = relPath;

    std::ofstream out(metaPath);
    if (!out.is_open()) {
        std::cerr << "[AssetMetaService] Failed to write: " << metaPath << std::endl;
        return;
    }
    out << node;
    std::cout << "[AssetMetaService] Generated: " << metaPath.filename() << std::endl;
}

// ──────────────────────────────────────────────────────────────────────────────
void AssetMetaService::GenerateFor(const std::string& sourceFile) {
    fs::path p(sourceFile);
    const std::string metaExt = MetaExtensionFor(p.extension().string());
    if (metaExt == ".texture")      ProcessTexture(p);
    else if (metaExt == ".shader")  ProcessShader(p);
    else if (metaExt == ".font")    ProcessFont(p);
    else if (metaExt == ".audio")   ProcessAudio(p);
}

// ──────────────────────────────────────────────────────────────────────────────
void AssetMetaService::ScanAndGenerate(const std::string& rootDir) {
    fs::path root(rootDir);
    if (!fs::exists(root) || !fs::is_directory(root)) {
        std::cerr << "[AssetMetaService] ScanAndGenerate: invalid directory: "
                  << rootDir << std::endl;
        return;
    }

    std::error_code ec;
    for (auto& entry : fs::recursive_directory_iterator(root, ec)) {
        if (!entry.is_regular_file()) continue;

        const fs::path& p   = entry.path();
        const std::string ext = p.extension().string();

        // Skip meta files themselves
        if (IsMetaExtension(ext)) continue;

        const std::string metaExt = MetaExtensionFor(ext);
        if (metaExt == ".texture") {
            ProcessTexture(p);
        } else if (metaExt == ".font") {
            ProcessFont(p);
        } else if (metaExt == ".shader") {
            ProcessShader(p);  // p is the .glsl file
        } else if (metaExt == ".audio") {
            ProcessAudio(p);
        }
    }
    if (ec) {
        std::cerr << "[AssetMetaService] Directory iteration error: " << ec.message() << std::endl;
    }
}
