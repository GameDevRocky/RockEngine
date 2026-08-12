#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// Turns a font file into an MSDF atlas + glyph table.
//
// This is the ONLY place in the engine that knows msdfgen exists. Everything
// downstream (Font, TextLayout, FontManager, the shader) sees the plain PODs
// below -- an RGB8 pixel buffer and a table of em-space quads. Swapping the
// generator, or moving to a fully pre-baked pipeline, touches this file alone.
// msdfgen is linked PRIVATE into RockEngineCore for the same reason; see
// Engine/CMakeLists.txt.
//
// UNITS. Everything spatial here is in EM units (1.0 == one em), never pixels
// and never world units. The em is the only scale that is a property of the
// typeface rather than of a particular TextRenderer, so it is what gets baked;
// multiplying by a component's fontSize is the last step, done at layout time.

// One glyph's baked placement.
struct BakedGlyph {
    std::uint32_t codepoint = 0;
    float advance = 0.0f;                  // pen movement after this glyph, em

    // The glyph quad relative to the pen origin sitting on the baseline, em.
    // All four are 0 for whitespace, which has an advance but no geometry.
    float planeLeft = 0.0f, planeBottom = 0.0f, planeRight = 0.0f, planeTop = 0.0f;

    // Where that quad lives in the atlas, normalized [0,1], y-up.
    float uvLeft = 0.0f, uvBottom = 0.0f, uvRight = 0.0f, uvTop = 0.0f;

    bool HasGeometry() const { return planeRight > planeLeft && planeTop > planeBottom; }
};

struct FontBakeSettings {
    // Atlas resolution of one em square, in pixels. This is the ONE scale applied
    // to every glyph in the face -- see the note on uniform scaling in Bake().
    int   emSize  = 48;
    // Width of the representable distance range, in atlas pixels. The shader needs
    // the same number to convert distances back to screen pixels, so it is baked
    // into the atlas and pushed as uPxRange rather than being a material property.
    float pxRange = 4.0f;
    // Codepoints to bake, as UTF-8. Empty means printable ASCII (U+0020..U+007E).
    std::string charset;
};

struct BakedAtlas {
    bool ok = false;
    std::string error;

    int width = 0, height = 0;
    std::vector<std::uint8_t> pixels;      // RGB8, tightly packed, row 0 == bottom

    std::vector<BakedGlyph> glyphs;

    // key = (uint64)left << 32 | right, value = em to add to the pen. Sparse: most
    // pairs have no kerning and are simply absent.
    std::vector<std::pair<std::uint64_t, float>> kerning;

    // Face-wide vertical metrics, em. descender is negative (below the baseline).
    float ascender = 0.0f, descender = 0.0f, lineHeight = 0.0f;
};

namespace FontAtlasBaker {

    // Load the font, generate an MSDF per glyph, shelf-pack them, and return the
    // atlas. Pure CPU work -- no GL, safe to call without a current context.
    // On failure returns ok == false with `error` set; never throws.
    //
    // COST, measured on a 95-glyph face at emSize 48: ~265 ms compiled with /O2,
    // but ~2000 ms in a Debug build, because the inner loop is per-pixel float
    // math over edge equations and MSVC Debug pessimizes exactly that. There is
    // deliberately no disk cache -- the result is rebuilt every session, so the
    // only thing keeping that cost off the clock is Font::EnsureUploaded's
    // laziness: a font nobody draws is never baked.
    BakedAtlas Bake(const std::string& fontPath, const FontBakeSettings& settings);

    // Decode UTF-8 into codepoints. Invalid bytes are skipped rather than
    // rejected -- a mangled charset should cost you a glyph, not the whole font.
    std::vector<std::uint32_t> DecodeUTF8(const std::string& text);
}
