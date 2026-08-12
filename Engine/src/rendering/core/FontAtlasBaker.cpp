#include "engine/rendering/core/FontAtlasBaker.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

// The one and only inclusion of msdfgen in the engine. See the header.
#include <msdfgen.h>
#include <msdfgen-ext.h>

namespace {

// Atlas widths we are willing to use. Kept power-of-two out of habit rather than
// necessity (GL 4.6 has no NPOT restriction), because it keeps the shelf
// arithmetic tidy and the texture sizes predictable in the inspector.
constexpr int kMinAtlasWidth = 128;
constexpr int kMaxAtlasWidth = 8192;

// Gap between packed glyphs, in pixels. Distance fields bleed past the glyph's
// own bounds by design, so touching neighbours would sample each other's field
// and produce halos along the edges where a linear filter tap crosses over.
constexpr int kGlyphPadding = 2;

// A glyph that has been rasterized and is waiting for a home in the atlas.
struct PendingGlyph {
    std::uint32_t codepoint = 0;
    float advance = 0.0f;
    double planeLeft = 0, planeBottom = 0, planeRight = 0, planeTop = 0;
    int width = 0, height = 0;
    std::vector<std::uint8_t> rgb;   // width*height*3, row 0 == bottom
    int atlasX = 0, atlasY = 0;      // filled in by the packer
};

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
std::vector<std::uint32_t> FontAtlasBaker::DecodeUTF8(const std::string& text) {
    std::vector<std::uint32_t> out;
    out.reserve(text.size());

    const unsigned char* p = reinterpret_cast<const unsigned char*>(text.data());
    const unsigned char* end = p + text.size();

    while (p < end) {
        std::uint32_t cp = 0;
        int extra = 0;
        if (*p < 0x80)            { cp = *p;        extra = 0; }
        else if ((*p & 0xE0) == 0xC0) { cp = *p & 0x1F; extra = 1; }
        else if ((*p & 0xF0) == 0xE0) { cp = *p & 0x0F; extra = 2; }
        else if ((*p & 0xF8) == 0xF0) { cp = *p & 0x07; extra = 3; }
        else { ++p; continue; }   // stray continuation byte -- skip it

        if (p + extra >= end) break;
        ++p;
        bool valid = true;
        for (int i = 0; i < extra; ++i, ++p) {
            if ((*p & 0xC0) != 0x80) { valid = false; break; }
            cp = (cp << 6) | (*p & 0x3F);
        }
        if (valid) out.push_back(cp);
    }
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
BakedAtlas FontAtlasBaker::Bake(const std::string& fontPath,
                                const FontBakeSettings& settings) {
    BakedAtlas result;

    const int emSize = std::max(8, settings.emSize);
    const double pxRange = std::max(1.0, static_cast<double>(settings.pxRange));

    msdfgen::FreetypeHandle* ft = msdfgen::initializeFreetype();
    if (!ft) { result.error = "FreeType failed to initialize"; return result; }

    msdfgen::FontHandle* face = msdfgen::loadFont(ft, fontPath.c_str());
    if (!face) {
        msdfgen::deinitializeFreetype(ft);
        result.error = "Could not load font: " + fontPath;
        return result;
    }

    // EM_NORMALIZED puts every metric on the 1.0 == one em scale the rest of the
    // engine expects. The LEGACY scaling msdfgen still defaults to divides by 64
    // and is documented as "do not use".
    msdfgen::FontMetrics fm{};
    if (msdfgen::getFontMetrics(fm, face, msdfgen::FONT_SCALING_EM_NORMALIZED)) {
        result.ascender   = static_cast<float>(fm.ascenderY);
        result.descender  = static_cast<float>(fm.descenderY);
        result.lineHeight = static_cast<float>(fm.lineHeight);
    }
    // Some faces report a zero or nonsense line height; fall back to something
    // typographically sane rather than stacking every line on the baseline.
    if (result.lineHeight <= 0.0f)
        result.lineHeight = (result.ascender - result.descender) > 0.0f
                          ? (result.ascender - result.descender) * 1.2f
                          : 1.2f;

    std::vector<std::uint32_t> codepoints;
    if (settings.charset.empty()) {
        for (std::uint32_t cp = 0x20; cp <= 0x7E; ++cp) codepoints.push_back(cp);
    } else {
        codepoints = DecodeUTF8(settings.charset);
        std::sort(codepoints.begin(), codepoints.end());
        codepoints.erase(std::unique(codepoints.begin(), codepoints.end()),
                         codepoints.end());
    }

    // ── Rasterize ───────────────────────────────────────────────────────────
    // UNIFORM SCALE, deliberately. The obvious alternative -- fit each glyph to a
    // fixed cell -- gives 'i' a far higher pixels-per-em than 'W', so the shader's
    // screenPxRange differs per glyph and stroke weights visibly disagree at small
    // sizes. One scale for the whole face is what keeps the type looking like type.
    const double emScale = static_cast<double>(emSize);
    const double rangeEm = pxRange / emScale;

    std::vector<PendingGlyph> pending;
    pending.reserve(codepoints.size());

    for (std::uint32_t cp : codepoints) {
        msdfgen::Shape shape;
        double advance = 0.0;
        if (!msdfgen::loadGlyph(shape, face, cp,
                                msdfgen::FONT_SCALING_EM_NORMALIZED, &advance))
            continue;   // not in the face

        PendingGlyph g;
        g.codepoint = cp;
        g.advance = static_cast<float>(advance);

        shape.normalize();
        if (shape.edgeCount() == 0) {
            // Whitespace: real advance, no geometry, no atlas footprint.
            pending.push_back(std::move(g));
            continue;
        }

        msdfgen::edgeColoringSimple(shape, 3.0);

        msdfgen::Shape::Bounds b = shape.getBounds();
        b.l -= rangeEm; b.b -= rangeEm;
        b.r += rangeEm; b.t += rangeEm;

        const int w = static_cast<int>(std::ceil((b.r - b.l) * emScale));
        const int h = static_cast<int>(std::ceil((b.t - b.b) * emScale));
        if (w <= 0 || h <= 0) { pending.push_back(std::move(g)); continue; }

        msdfgen::Bitmap<float, 3> bmp(w, h);
        msdfgen::SDFTransformation xf(
            msdfgen::Projection(msdfgen::Vector2(emScale, emScale),
                                msdfgen::Vector2(-b.l, -b.b)),
            msdfgen::DistanceMapping(msdfgen::Range(rangeEm)));
        msdfgen::generateMSDF(bmp, shape, xf, msdfgen::MSDFGeneratorConfig());

        g.planeLeft = b.l; g.planeBottom = b.b;
        g.planeRight = b.r; g.planeTop = b.t;
        g.width = w; g.height = h;
        g.rgb.resize(static_cast<size_t>(w) * h * 3);

        // msdfgen's default Y orientation is Y_UPWARD and bmp(x, 0) is the bottom
        // row, which is also what GL wants as the first row of texture data. No
        // flip anywhere in this pipeline -- if text ever renders upside down, this
        // assumption is where to look.
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const float* src = bmp(x, y);
                std::uint8_t* dst = &g.rgb[(static_cast<size_t>(y) * w + x) * 3];
                dst[0] = msdfgen::pixelFloatToByte(src[0]);
                dst[1] = msdfgen::pixelFloatToByte(src[1]);
                dst[2] = msdfgen::pixelFloatToByte(src[2]);
            }
        }
        pending.push_back(std::move(g));
    }

    // ── Kerning ─────────────────────────────────────────────────────────────
    for (std::uint32_t a : codepoints) {
        for (std::uint32_t b : codepoints) {
            double k = 0.0;
            if (!msdfgen::getKerning(k, face, a, b,
                                     msdfgen::FONT_SCALING_EM_NORMALIZED))
                continue;
            if (k == 0.0) continue;   // the overwhelming majority; keep it sparse
            result.kerning.emplace_back((static_cast<std::uint64_t>(a) << 32) | b,
                                        static_cast<float>(k));
        }
    }
    std::sort(result.kerning.begin(), result.kerning.end());

    msdfgen::destroyFont(face);
    msdfgen::deinitializeFreetype(ft);

    if (pending.empty()) {
        result.error = "Font contained none of the requested codepoints";
        return result;
    }

    // ── Shelf-pack ──────────────────────────────────────────────────────────
    // Tallest-first into rows. Not optimal packing, but the waste is a few percent
    // on a text face and the implementation has no failure modes worth debugging.
    std::vector<PendingGlyph*> order;
    order.reserve(pending.size());
    for (auto& g : pending) if (g.width > 0) order.push_back(&g);

    std::sort(order.begin(), order.end(), [](const PendingGlyph* a, const PendingGlyph* b) {
        if (a->height != b->height) return a->height > b->height;
        return a->codepoint < b->codepoint;   // stable across runs
    });

    long long totalArea = 0;
    int widest = 0;
    for (const auto* g : order) {
        totalArea += static_cast<long long>(g->width + kGlyphPadding) * (g->height + kGlyphPadding);
        widest = std::max(widest, g->width + kGlyphPadding);
    }

    int atlasW = kMinAtlasWidth;
    // Start from a square-ish guess, then grow until every glyph fits across.
    while (atlasW < kMaxAtlasWidth &&
           (static_cast<long long>(atlasW) * atlasW < totalArea * 5 / 4 || atlasW < widest))
        atlasW *= 2;

    int penX = kGlyphPadding, penY = kGlyphPadding, shelfHeight = 0, atlasH = 0;
    for (PendingGlyph* g : order) {
        if (penX + g->width + kGlyphPadding > atlasW) {
            penX = kGlyphPadding;
            penY += shelfHeight + kGlyphPadding;
            shelfHeight = 0;
        }
        g->atlasX = penX;
        g->atlasY = penY;
        penX += g->width + kGlyphPadding;
        shelfHeight = std::max(shelfHeight, g->height);
        atlasH = std::max(atlasH, penY + g->height + kGlyphPadding);
    }

    // Round up so the texture stays a tidy size; the slack is never sampled.
    int potH = kMinAtlasWidth;
    while (potH < atlasH) potH *= 2;
    atlasH = std::max(potH, kMinAtlasWidth);

    result.width = atlasW;
    result.height = atlasH;
    // Zero = a distance of -0.5 in every channel, i.e. solidly outside the glyph.
    // Any stray sample landing in the gutter therefore reads as empty, not as ink.
    result.pixels.assign(static_cast<size_t>(atlasW) * atlasH * 3, 0);

    for (const PendingGlyph* g : order) {
        for (int y = 0; y < g->height; ++y) {
            const std::uint8_t* src = &g->rgb[(static_cast<size_t>(y) * g->width) * 3];
            std::uint8_t* dst = &result.pixels[
                (static_cast<size_t>(g->atlasY + y) * atlasW + g->atlasX) * 3];
            std::memcpy(dst, src, static_cast<size_t>(g->width) * 3);
        }
    }

    // ── Emit the table ──────────────────────────────────────────────────────
    result.glyphs.reserve(pending.size());
    for (const PendingGlyph& g : pending) {
        BakedGlyph out;
        out.codepoint = g.codepoint;
        out.advance = g.advance;
        if (g.width > 0) {
            out.planeLeft   = static_cast<float>(g.planeLeft);
            out.planeBottom = static_cast<float>(g.planeBottom);
            out.planeRight  = static_cast<float>(g.planeRight);
            out.planeTop    = static_cast<float>(g.planeTop);
            out.uvLeft   = static_cast<float>(g.atlasX)            / atlasW;
            out.uvBottom = static_cast<float>(g.atlasY)            / atlasH;
            out.uvRight  = static_cast<float>(g.atlasX + g.width)  / atlasW;
            out.uvTop    = static_cast<float>(g.atlasY + g.height) / atlasH;
        }
        result.glyphs.push_back(out);
    }
    std::sort(result.glyphs.begin(), result.glyphs.end(),
              [](const BakedGlyph& a, const BakedGlyph& b) { return a.codepoint < b.codepoint; });

    result.ok = true;
    return result;
}
