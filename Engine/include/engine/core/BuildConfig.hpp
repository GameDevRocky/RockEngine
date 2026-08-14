#pragma once

#include <string>

// Everything a shipped game needs to know about itself at startup.
//
// Lives in Engine rather than Editor because BOTH sides need it and only one of them can
// link Qt: the editor's Build window authors it, the player reads it. One struct, one
// schema, no chance of the writer and the reader drifting apart.
//
// Deliberately NOT a Serializable: it is never inspected, never copied into a play-mode
// container, and never registered with SerializableFactory. It is a config file, not a
// runtime object, so plain yaml-cpp read/write is the honest shape.
//
// Two files use this schema:
//   Domain/sandbox/project.build   -- authored settings, committed with the project
//   <output>/game.rock             -- the copy written beside the shipped executable
struct BuildConfig {
    std::string gameName = "My Game";
    std::string version  = "1.0.0";

    // Asset-relative (e.g. "Domain/sandbox/default.scene"), stored through
    // EngineUtils::ToAssetRelative and resolved through GetAssetPath. An absolute path
    // here would be a path on the build machine and would resolve nowhere else.
    std::string startupScene;

    int  windowWidth  = 1280;
    int  windowHeight = 720;
    bool fullscreen   = false;
    bool resizable    = true;
    bool vsync        = true;

    // Reads `path` (an absolute filesystem path). Returns false and leaves `out` at its
    // defaults if the file is missing or unparseable -- a corrupt config should give you a
    // playable default-sized window and a clear log line, not a failure to launch.
    static bool Load(const std::string& path, BuildConfig& out);

    // Writes `path`, creating parent directories as needed. Returns false on I/O failure.
    static bool Save(const std::string& path, const BuildConfig& cfg);
};
