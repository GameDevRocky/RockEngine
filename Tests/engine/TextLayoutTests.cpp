// TextLayout advertises itself as "pure text shaping: no GL, no msdfgen, no engine
// singletons", which makes it the one rendering-adjacent thing reachable from a headless
// test. Without a baked Font there is no glyph metric to lay anything out against, so what
// is testable here is the degenerate contract -- and that is worth having, because both the
// editor's pick bounds and the mesh cache call these on text that is routinely empty or
// attached to a font that has not finished baking.

#include <doctest.h>

#include <string>

#include "engine/text/TextLayout.hpp"

TEST_CASE("Build returns an empty mesh for a null font") {
    // A TextRenderer whose font is still loading hits this every frame until the atlas
    // bake finishes. It must be a no-op, not a crash.
    const TextMesh mesh = TextLayout::Build(nullptr, "Hello", TextLayoutSpec{});

    CHECK(mesh.quads.empty());
    CHECK(mesh.lineCount == 0);
    CHECK(mesh.boundsMin == glm::vec2(0.0f));
    CHECK(mesh.boundsMax == glm::vec2(0.0f));
}

TEST_CASE("Build returns an empty mesh for an empty string") {
    const TextMesh mesh = TextLayout::Build(nullptr, "", TextLayoutSpec{});

    CHECK(mesh.quads.empty());
    CHECK(mesh.lineCount == 0);
}

TEST_CASE("Build tolerates degenerate layout specs") {
    TextLayoutSpec spec;
    spec.fontSize      = 0.0f;
    spec.lineSpacing   = 0.0f;
    spec.letterSpacing = -10.0f;
    spec.maxWidth      = -1.0f;   // negative wrap width

    // Every one of these is reachable from the inspector, which lets you type any float.
    CHECK_NOTHROW(TextLayout::Build(nullptr, "text", spec));
}

TEST_CASE("Build handles every alignment combination without a font") {
    for (auto h : {TextHAlign::Left, TextHAlign::Center, TextHAlign::Right}) {
        for (auto v : {TextVAlign::Top, TextVAlign::Middle,
                       TextVAlign::Baseline, TextVAlign::Bottom}) {
            TextLayoutSpec spec;
            spec.hAlign = h;
            spec.vAlign = v;
            CHECK_NOTHROW(TextLayout::Build(nullptr, "aligned", spec));
        }
    }
}

TEST_CASE("Measure writes zeroed bounds for a null font") {
    // Measure is what the editor's gizmo and picking use. Leaving the out-params untouched
    // would hand the caller uninitialised stack, so it must write zeros explicitly.
    glm::vec2 min{ 99.0f, 99.0f };
    glm::vec2 max{ 99.0f, 99.0f };

    TextLayout::Measure(nullptr, "Hello", TextLayoutSpec{}, min, max);

    CHECK(min == glm::vec2(0.0f));
    CHECK(max == glm::vec2(0.0f));
}

TEST_CASE("Measure agrees with Build's bounds") {
    glm::vec2 min{ 0.0f }, max{ 0.0f };
    TextLayout::Measure(nullptr, "Hello", TextLayoutSpec{}, min, max);

    const TextMesh mesh = TextLayout::Build(nullptr, "Hello", TextLayoutSpec{});

    // Two entry points, one layout. If they ever disagree the gizmo box stops matching the
    // glyphs it is drawn around.
    CHECK(min == mesh.boundsMin);
    CHECK(max == mesh.boundsMax);
}
