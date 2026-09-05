#pragma once

#include <filesystem>
#include <fstream>
#include <string>

#include "engine/utils/EngineUtils.hpp"

namespace testsupport {

// An RAII scratch directory under the OS temp dir, removed on destruction.
//
// Tests that exercise real file I/O (BuildConfig::Save/Load, AssetMetaService) need a
// writable path that is NOT inside the repo -- a test that leaves droppings in Domain/
// shows up as dirty git status and, worse, can be picked up by the next asset scan.
class TempDir {
public:
    TempDir() {
        // GenerateUUID keeps parallel ctest invocations from colliding on one path.
        path_ = std::filesystem::temp_directory_path() /
                ("rockengine-tests-" + EngineUtils::GenerateUUID());
        std::filesystem::create_directories(path_);
    }

    ~TempDir() {
        std::error_code ec;                            // never throw out of a destructor
        std::filesystem::remove_all(path_, ec);
    }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    const std::filesystem::path& Path() const { return path_; }

    // Absolute path to `name` inside this directory, as the std::string the engine APIs take.
    std::string File(const std::string& name) const {
        return (path_ / name).string();
    }

    // Create a file with the given contents; parent directories are created as needed.
    std::string Write(const std::string& name, const std::string& contents) const {
        const std::filesystem::path full = path_ / name;
        std::filesystem::create_directories(full.parent_path());
        std::ofstream out(full, std::ios::binary);
        out << contents;
        return full.string();
    }

private:
    std::filesystem::path path_;
};

} // namespace testsupport
