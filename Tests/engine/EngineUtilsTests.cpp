// EngineUtils is small, pure, and load-bearing in ways that are easy to miss: GenerateUUID
// is the identity of every Serializable ever created, and the pixels<->meters pair is the
// single boundary between the visual world (1 unit == 1 pixel) and Box2D (which solves in
// meters). A silent change to either shows up as "physics feels wrong" rather than an error.

#include <doctest.h>

#include <set>
#include <string>

#include "engine/utils/EngineUtils.hpp"

TEST_CASE("GenerateUUID returns 32 lowercase hex characters") {
    const std::string id = EngineUtils::GenerateUUID();

    // IdRemapper's safety argument depends on this shape: it rewrites any scalar matching a
    // mapped id, which is only safe while ids are unmistakable 32-hex strings.
    CHECK(id.size() == 32);
    for (char c : id) {
        CAPTURE(id);
        CHECK(((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')));
    }
}

TEST_CASE("GenerateUUID does not collide over many calls") {
    std::set<std::string> seen;
    for (int i = 0; i < 10000; ++i)
        seen.insert(EngineUtils::GenerateUUID());

    // A duplicate id means two objects share a Registry slot -- one silently replaces the
    // other on the next scene load.
    CHECK(seen.size() == 10000);
}

TEST_CASE("pixels and meters convert at the documented physics scale") {
    using namespace EngineUtils::RenderUtils;

    // 32 px per meter. This constant is the entire Box2D boundary; changing it rescales
    // gravity, velocities and every collider in every existing scene at once.
    CHECK(PixelsPerMeter == doctest::Approx(32.0f));

    CHECK(PixelsToMeters(32.0f)  == doctest::Approx(1.0f));
    CHECK(MetersToPixels(1.0f)   == doctest::Approx(32.0f));
    CHECK(PixelsToMeters(MetersToPixels(2.5f)) == doctest::Approx(2.5f));

    const glm::vec2 pixels(64.0f, 16.0f);
    const glm::vec2 meters = PixelsToMeters(pixels);
    CHECK(meters.x == doctest::Approx(2.0f));
    CHECK(meters.y == doctest::Approx(0.5f));

    const glm::vec2 back = MetersToPixels(meters);
    CHECK(back.x == doctest::Approx(pixels.x));
    CHECK(back.y == doctest::Approx(pixels.y));
}

TEST_CASE("pixel and world units are the same thing") {
    using namespace EngineUtils::RenderUtils;

    // Deliberately identity -- kept so call sites read as an explicit crossing. If these
    // ever stop being identity, every sprite size in the engine changes meaning.
    CHECK(PixelsToWorld(123.0f) == doctest::Approx(123.0f));
    CHECK(WorldToPixels(123.0f) == doctest::Approx(123.0f));
}

TEST_CASE("degree and radian constants are consistent") {
    using namespace EngineUtils::MathUtils;

    CHECK(180.0f * DEG_2_RAD == doctest::Approx(PI));
    CHECK(PI * RAD_2_DEG     == doctest::Approx(180.0f));
    CHECK(DEG_2_RAD * RAD_2_DEG == doctest::Approx(1.0f));
}

TEST_CASE("engine-managed texture slots do not collide") {
    using namespace EngineUtils::TextureSlots;

    // These sit high and fixed so a material that grows one more sampler cannot stomp an
    // engine-bound texture. Distinctness is the whole guarantee.
    CHECK(NormalMap   != ShadowAtlas);
    CHECK(ShadowAtlas != FontAtlas);
    CHECK(NormalMap   != FontAtlas);
}

TEST_CASE("ToAssetRelative inverts GetAssetPath") {
    const std::string relative = "Domain/sandbox/default.scene";
    const std::string absolute = EngineUtils::GetAssetPath(relative);

    CHECK(EngineUtils::ToAssetRelative(absolute) == relative);
}

TEST_CASE("ToAssetRelative leaves a path outside the asset root unchanged") {
#if defined(_WIN32)
    const std::string outside = "C:/definitely/not/in/the/project/file.txt";
#else
    const std::string outside = "/definitely/not/in/the/project/file.txt";
#endif
    // BuildConfig stores an asset-relative startupScene precisely because an absolute path
    // would be a path on the build machine. Silently mangling an outside path here would
    // produce a startupScene that resolves nowhere.
    CHECK(EngineUtils::ToAssetRelative(outside) == outside);
}
