// BuildConfig is the schema shared by the editor's Build window (which writes
// Domain/sandbox/project.build) and the shipped player (which reads game.rock beside its
// own executable). A regression here breaks every game build, silently, at launch -- and
// it is plain yaml-cpp with no engine state, so it is also the cheapest thing to pin down.

#include <doctest.h>

#include <filesystem>

#include "engine/core/BuildConfig.hpp"
#include "support/TempDir.hpp"

using testsupport::TempDir;

TEST_CASE("BuildConfig round-trips every field through save/load") {
    TempDir dir;
    const std::string path = dir.File("game.rock");

    BuildConfig written;
    written.gameName     = "Round Trip";
    written.version      = "2.5.1";
    written.startupScene = "Domain/sandbox/default.scene";
    written.windowWidth  = 1920;
    written.windowHeight = 1080;
    written.fullscreen   = true;
    written.resizable    = false;
    written.vsync        = false;

    REQUIRE(BuildConfig::Save(path, written));
    REQUIRE(std::filesystem::exists(path));

    BuildConfig read;
    REQUIRE(BuildConfig::Load(path, read));

    CHECK(read.gameName     == written.gameName);
    CHECK(read.version      == written.version);
    CHECK(read.startupScene == written.startupScene);
    CHECK(read.windowWidth  == written.windowWidth);
    CHECK(read.windowHeight == written.windowHeight);
    // The three bools matter most: `false` is the default for fullscreen and the default
    // for neither resizable nor vsync, so a writer that drops a field looks correct on
    // fullscreen alone.
    CHECK(read.fullscreen   == written.fullscreen);
    CHECK(read.resizable    == written.resizable);
    CHECK(read.vsync        == written.vsync);
}

TEST_CASE("BuildConfig::Save creates missing parent directories") {
    TempDir dir;
    // The Build window writes into an output folder the user just named, which usually
    // does not exist yet.
    const std::string path = dir.File("nested/deeper/game.rock");

    BuildConfig cfg;
    cfg.gameName = "Nested";

    REQUIRE(BuildConfig::Save(path, cfg));
    CHECK(std::filesystem::exists(path));
}

TEST_CASE("BuildConfig::Load leaves defaults intact when the file is missing") {
    TempDir dir;

    BuildConfig cfg;
    const BuildConfig defaults;

    CHECK_FALSE(BuildConfig::Load(dir.File("does-not-exist.rock"), cfg));

    // Documented contract: a config that cannot be read gives a playable default window,
    // not a mangled one.
    CHECK(cfg.gameName     == defaults.gameName);
    CHECK(cfg.windowWidth  == defaults.windowWidth);
    CHECK(cfg.windowHeight == defaults.windowHeight);
}

TEST_CASE("BuildConfig::Load reports failure on unparseable YAML without throwing") {
    TempDir dir;
    // Deliberately broken YAML: an unclosed flow mapping. yaml-cpp throws on this, and
    // the throw must not escape Load -- a corrupt config should log and fall back, not
    // take the game process down before it draws a frame.
    const std::string path = dir.Write("broken.rock", "gameName: [unclosed\n\t\tbad: :\n");

    BuildConfig cfg;
    bool ok = true;
    CHECK_NOTHROW(ok = BuildConfig::Load(path, cfg));
    CHECK_FALSE(ok);
}

TEST_CASE("BuildConfig::Load reads the project.build committed in the repo") {
    // Guards the real fixture the editor ships with: if the schema drifts, this fails
    // even when a synthetic round trip still passes.
    const std::string path = EngineUtils::GetAssetPath("Domain/sandbox/project.build");
    if (!std::filesystem::exists(path)) {
        MESSAGE("Domain/sandbox/project.build not present; skipping");
        return;
    }

    BuildConfig cfg;
    REQUIRE(BuildConfig::Load(path, cfg));
    CHECK_FALSE(cfg.gameName.empty());
    CHECK(cfg.windowWidth  > 0);
    CHECK(cfg.windowHeight > 0);
}
