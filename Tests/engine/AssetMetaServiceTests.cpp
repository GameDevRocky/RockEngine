// The meta-file convention is an implicit contract between four places: AssetMetaService
// (which generates the files), AssetManager (which scans for them), the Folder view (which
// decides what to show), and GameBuilder (which ships them). The mapping table is the only
// thing keeping them agreeing, and it is pure string work -- so pinning it costs nothing
// and catches a source extension being added to one side but not the other.

#include <doctest.h>

#include <string>

#include "engine/rendering/core/AssetMetaService.hpp"

TEST_CASE("source extensions map to their documented meta extension") {
    // Images -> .texture
    CHECK(AssetMetaService::MetaExtensionFor(".png")  == ".texture");
    CHECK(AssetMetaService::MetaExtensionFor(".jpg")  == ".texture");

    CHECK(AssetMetaService::MetaExtensionFor(".jpeg") == ".texture");
    CHECK(AssetMetaService::MetaExtensionFor(".bmp")  == ".texture");

    // Shaders -> .shader. One combined .glsl file split by #pragma vertex / #pragma
    // fragment markers (see EngineUtils::ParseShaderSource) -- NOT a .vert/.frag pair.
    CHECK(AssetMetaService::MetaExtensionFor(".glsl") == ".shader");

    // Fonts -> .font
    CHECK(AssetMetaService::MetaExtensionFor(".ttf")  == ".font");
    CHECK(AssetMetaService::MetaExtensionFor(".otf")  == ".font");

    // Audio -> .audio
    CHECK(AssetMetaService::MetaExtensionFor(".wav")  == ".audio");
    CHECK(AssetMetaService::MetaExtensionFor(".mp3")  == ".audio");
    CHECK(AssetMetaService::MetaExtensionFor(".ogg")  == ".audio");
    CHECK(AssetMetaService::MetaExtensionFor(".flac") == ".audio");
}

TEST_CASE("an unknown source extension maps to nothing") {
    // Documented as returning "" rather than guessing -- a dropped .txt must not acquire a
    // meta file and start being treated as an asset.
    CHECK(AssetMetaService::MetaExtensionFor(".txt").empty());
    CHECK(AssetMetaService::MetaExtensionFor(".cpp").empty());
    // Separate .vert/.frag files are not the convention; only combined .glsl is.
    CHECK(AssetMetaService::MetaExtensionFor(".vert").empty());
    CHECK(AssetMetaService::MetaExtensionFor(".frag").empty());
    CHECK(AssetMetaService::MetaExtensionFor("").empty());
}

TEST_CASE("meta extensions are recognised as meta") {
    CHECK(AssetMetaService::IsMetaExtension(".texture"));
    CHECK(AssetMetaService::IsMetaExtension(".shader"));
    CHECK(AssetMetaService::IsMetaExtension(".material"));
    CHECK(AssetMetaService::IsMetaExtension(".font"));
    CHECK(AssetMetaService::IsMetaExtension(".audio"));
}

TEST_CASE("source extensions are not mistaken for meta extensions") {
    // The two predicates must stay disjoint over source files: a .png treated as a meta
    // file would be parsed as YAML and fail confusingly.
    CHECK_FALSE(AssetMetaService::IsMetaExtension(".glsl"));
    CHECK_FALSE(AssetMetaService::IsMetaExtension(".png"));
    CHECK_FALSE(AssetMetaService::IsMetaExtension(".ttf"));
    CHECK_FALSE(AssetMetaService::IsMetaExtension(".wav"));
    CHECK_FALSE(AssetMetaService::IsMetaExtension(".txt"));
    CHECK_FALSE(AssetMetaService::IsMetaExtension(""));
}

TEST_CASE("every generated meta extension is itself recognised as meta") {
    // The round trip that actually matters: generate a meta for a source file, then the
    // scanner must classify what was produced as a meta file. If these two tables drift,
    // freshly generated assets become invisible to AssetManager.
    for (const std::string sourceExt : {".png", ".jpg", ".jpeg", ".bmp", ".glsl", ".ttf",
                                        ".otf", ".wav", ".mp3", ".ogg", ".flac"}) {
        CAPTURE(sourceExt);
        const std::string meta = AssetMetaService::MetaExtensionFor(sourceExt);
        REQUIRE_FALSE(meta.empty());
        CHECK(AssetMetaService::IsMetaExtension(meta));
    }
}
