#include "engine/core/BuildConfig.hpp"

#include <yaml-cpp/yaml.h>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

bool BuildConfig::Load(const std::string& path, BuildConfig& out) {
    std::error_code ec;
    if (!fs::exists(path, ec)) return false;

    YAML::Node root;
    try {
        root = YAML::LoadFile(path);
    } catch (const std::exception& e) {
        std::cerr << "BuildConfig: failed to parse " << path << ": " << e.what() << std::endl;
        return false;
    }
    if (!root || !root.IsMap()) return false;

    // as<T>(fallback) throughout, so a config written by an older build (or hand-edited
    // with a field removed) still loads, with the missing field taking its default rather
    // than throwing.
    out.gameName     = root["gameName"].as<std::string>(out.gameName);
    out.version      = root["version"].as<std::string>(out.version);
    out.startupScene = root["startupScene"].as<std::string>(out.startupScene);
    out.windowWidth  = root["windowWidth"].as<int>(out.windowWidth);
    out.windowHeight = root["windowHeight"].as<int>(out.windowHeight);
    out.fullscreen   = root["fullscreen"].as<bool>(out.fullscreen);
    out.resizable    = root["resizable"].as<bool>(out.resizable);
    out.vsync        = root["vsync"].as<bool>(out.vsync);

    // A zero or negative window size makes SDL_CreateWindow fail outright, which would
    // read as "the game is broken" rather than "the config is wrong".
    if (out.windowWidth  < 1) out.windowWidth  = 1280;
    if (out.windowHeight < 1) out.windowHeight = 720;

    return true;
}

bool BuildConfig::Save(const std::string& path, const BuildConfig& cfg) {
    std::error_code ec;
    const fs::path parent = fs::path(path).parent_path();
    if (!parent.empty()) fs::create_directories(parent, ec);

    YAML::Emitter emit;
    emit << YAML::BeginMap;
    emit << YAML::Key << "gameName"     << YAML::Value << cfg.gameName;
    emit << YAML::Key << "version"      << YAML::Value << cfg.version;
    emit << YAML::Key << "startupScene" << YAML::Value << cfg.startupScene;
    emit << YAML::Key << "windowWidth"  << YAML::Value << cfg.windowWidth;
    emit << YAML::Key << "windowHeight" << YAML::Value << cfg.windowHeight;
    emit << YAML::Key << "fullscreen"   << YAML::Value << cfg.fullscreen;
    emit << YAML::Key << "resizable"    << YAML::Value << cfg.resizable;
    emit << YAML::Key << "vsync"        << YAML::Value << cfg.vsync;
    emit << YAML::EndMap;

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        std::cerr << "BuildConfig: cannot write " << path << std::endl;
        return false;
    }
    file << emit.c_str() << "\n";
    return file.good();
}
