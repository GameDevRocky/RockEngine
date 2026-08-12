#include "engine/text/TextLayout.hpp"

#include <algorithm>

#include "engine/rendering/core/Font.hpp"
#include "engine/rendering/core/FontAtlasBaker.hpp"

namespace {

// A run of codepoints that will occupy one visual line, plus its measured width
// in em. Produced by the wrapper; consumed by the emitter.
struct Line {
    size_t begin = 0, end = 0;   // half-open range into the codepoint vector
    float  width = 0.0f;         // em
};

// Width of one glyph, em, ignoring its neighbours.
float GlyphAdvance(const Font* font, std::uint32_t cp) {
    const BakedGlyph* g = font->GetGlyph(cp);
    return g ? g->advance : 0.0f;
}

// What sits *between* two adjacent glyphs: the face's kerning pair plus tracking.
// Splitting the run width this way (rather than folding kerning into the previous
// glyph's advance) is what lets the wrapper below accumulate incrementally.
float GapBetween(const Font* font, std::uint32_t left, std::uint32_t right, float letterSpacing) {
    return font->GetKerning(left, right) + letterSpacing;
}

// Width of a codepoint range, em.
float MeasureRun(const Font* font, const std::vector<std::uint32_t>& cps,
                 size_t begin, size_t end, float letterSpacing) {
    float w = 0.0f;
    for (size_t i = begin; i < end; ++i) {
        if (i > begin) w += GapBetween(font, cps[i - 1], cps[i], letterSpacing);
        w += GlyphAdvance(font, cps[i]);
    }
    return w;
}

// Greedy word wrap of one hard line (already split on '\n') into visual lines.
// Width is accumulated as the scan advances rather than re-measured per
// character, so wrapping a long paragraph stays linear.
void WrapLine(const Font* font, const std::vector<std::uint32_t>& cps,
              size_t begin, size_t end, float maxWidthEm, float letterSpacing,
              std::vector<Line>& out) {
    if (maxWidthEm <= 0.0f) {
        out.push_back({ begin, end, MeasureRun(font, cps, begin, end, letterSpacing) });
        return;
    }

    size_t lineStart = begin;
    size_t lastBreak = std::string::npos;   // last space we could break at
    float  width     = 0.0f;                // width of [lineStart, i)

    for (size_t i = begin; i < end; ++i) {
        if (cps[i] == ' ') lastBreak = i;

        const float gap = (i > lineStart) ? GapBetween(font, cps[i - 1], cps[i], letterSpacing) : 0.0f;
        const float candidate = width + gap + GlyphAdvance(font, cps[i]);

        if (candidate <= maxWidthEm || i == lineStart) {
            width = candidate;
            continue;
        }

        if (lastBreak != std::string::npos && lastBreak > lineStart) {
            // Break at the space: it ends the line and is not carried onto the next.
            out.push_back({ lineStart, lastBreak,
                            MeasureRun(font, cps, lineStart, lastBreak, letterSpacing) });
            lineStart = lastBreak + 1;
        } else {
            // A single word longer than the whole line. Hard-break it rather than
            // let it run off forever -- an unbroken 200-character token is a bug
            // in the content, but silently ignoring maxWidth is a worse answer.
            out.push_back({ lineStart, i,
                            MeasureRun(font, cps, lineStart, i, letterSpacing) });
            lineStart = i;
        }
        lastBreak = std::string::npos;
        // Restart the accumulator on the character that forced the break; it is
        // the first glyph of the new line.
        width = MeasureRun(font, cps, lineStart, i + 1, letterSpacing);
    }

    out.push_back({ lineStart, end, MeasureRun(font, cps, lineStart, end, letterSpacing) });
}

// Shared front half of Build and Measure: decode, wrap, and resolve the vertical
// origin. Returns false when there is nothing to lay out.
bool Prepare(const Font* font, const std::string& utf8, const TextLayoutSpec& spec,
             std::vector<std::uint32_t>& cps, std::vector<Line>& lines,
             float& lineAdvanceEm, float& firstBaselineEm, float& widestEm) {
    if (!font || !font->IsReady() || utf8.empty()) return false;

    cps = FontAtlasBaker::DecodeUTF8(utf8);
    if (cps.empty()) return false;

    const float emPerUnit = spec.fontSize > 0.0f ? spec.fontSize : 1.0f;
    const float maxWidthEm = spec.maxWidth > 0.0f ? spec.maxWidth / emPerUnit : 0.0f;

    // Split on newlines first; each hard line wraps independently.
    size_t lineStart = 0;
    for (size_t i = 0; i <= cps.size(); ++i) {
        if (i == cps.size() || cps[i] == '\n') {
            WrapLine(font, cps, lineStart, i, maxWidthEm, spec.letterSpacing, lines);
            lineStart = i + 1;
        }
    }
    if (lines.empty()) return false;

    lineAdvanceEm = font->GetLineHeight() * spec.lineSpacing;

    widestEm = 0.0f;
    for (const Line& l : lines) widestEm = std::max(widestEm, l.width);

    const float ascender  = font->GetAscender();
    const float descender = font->GetDescender();     // negative
    const float stack = static_cast<float>(lines.size() - 1) * lineAdvanceEm;

    switch (spec.vAlign) {
        case TextVAlign::Top:      firstBaselineEm = -ascender; break;
        case TextVAlign::Baseline: firstBaselineEm = 0.0f; break;
        case TextVAlign::Bottom:   firstBaselineEm = stack - descender; break;
        case TextVAlign::Middle:
        default:                   firstBaselineEm = (stack - ascender - descender) * 0.5f; break;
    }
    return true;
}

// Pen x where a line of the given width starts, em. Each line is aligned against
// its own width, not the block's, which is what makes centred multi-line text
// centre per line rather than left-align inside a centred box.
float LineStartX(float lineWidthEm, TextHAlign align) {
    switch (align) {
        case TextHAlign::Left:  return 0.0f;
        case TextHAlign::Right: return -lineWidthEm;
        case TextHAlign::Center:
        default:                return -lineWidthEm * 0.5f;
    }
}

// Horizontal extent of the whole block, em, for the given alignment.
void BlockXExtent(float widestEm, TextHAlign align, float& outMin, float& outMax) {
    switch (align) {
        case TextHAlign::Left:  outMin = 0.0f;             outMax = widestEm;       break;
        case TextHAlign::Right: outMin = -widestEm;        outMax = 0.0f;           break;
        case TextHAlign::Center:
        default:                outMin = -widestEm * 0.5f; outMax = widestEm * 0.5f; break;
    }
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
TextMesh TextLayout::Build(const Font* font, const std::string& utf8,
                           const TextLayoutSpec& spec) {
    TextMesh mesh;

    std::vector<std::uint32_t> cps;
    std::vector<Line> lines;
    float lineAdvanceEm = 0.0f, firstBaselineEm = 0.0f, widestEm = 0.0f;
    if (!Prepare(font, utf8, spec, cps, lines, lineAdvanceEm, firstBaselineEm, widestEm))
        return mesh;

    const float scale = spec.fontSize;
    mesh.lineCount = static_cast<int>(lines.size());
    mesh.quads.reserve(cps.size());

    for (size_t li = 0; li < lines.size(); ++li) {
        const Line& line = lines[li];
        const float baselineEm = firstBaselineEm - static_cast<float>(li) * lineAdvanceEm;
        float penEm = LineStartX(line.width, spec.hAlign);

        for (size_t i = line.begin; i < line.end; ++i) {
            const std::uint32_t cp = cps[i];
            if (i > line.begin)
                penEm += GapBetween(font, cps[i - 1], cp, spec.letterSpacing);

            const BakedGlyph* g = font->GetGlyph(cp);

            // Whitespace and unmapped codepoints move the pen and emit nothing.
            // Not an error worth logging every frame -- the gap in the text is
            // the report.
            if (g && g->HasGeometry()) {
                TextQuad q;
                q.min = { (penEm + g->planeLeft)  * scale, (baselineEm + g->planeBottom) * scale };
                q.max = { (penEm + g->planeRight) * scale, (baselineEm + g->planeTop)    * scale };
                q.uvMin = { g->uvLeft,  g->uvBottom };
                q.uvMax = { g->uvRight, g->uvTop };
                mesh.quads.push_back(q);
            }
            penEm += GlyphAdvance(font, cp);
        }
    }

    float xMin = 0.0f, xMax = 0.0f;
    BlockXExtent(widestEm, spec.hAlign, xMin, xMax);
    const float topEm    = firstBaselineEm + font->GetAscender();
    const float bottomEm = firstBaselineEm
                         - static_cast<float>(lines.size() - 1) * lineAdvanceEm
                         + font->GetDescender();

    mesh.boundsMin = { xMin * scale, bottomEm * scale };
    mesh.boundsMax = { xMax * scale, topEm * scale };
    return mesh;
}

// ─────────────────────────────────────────────────────────────────────────────
void TextLayout::Measure(const Font* font, const std::string& utf8,
                         const TextLayoutSpec& spec,
                         glm::vec2& outMin, glm::vec2& outMax) {
    outMin = glm::vec2(0.0f);
    outMax = glm::vec2(0.0f);

    std::vector<std::uint32_t> cps;
    std::vector<Line> lines;
    float lineAdvanceEm = 0.0f, firstBaselineEm = 0.0f, widestEm = 0.0f;
    if (!Prepare(font, utf8, spec, cps, lines, lineAdvanceEm, firstBaselineEm, widestEm))
        return;

    const float scale = spec.fontSize;
    float xMin = 0.0f, xMax = 0.0f;
    BlockXExtent(widestEm, spec.hAlign, xMin, xMax);
    const float topEm    = firstBaselineEm + font->GetAscender();
    const float bottomEm = firstBaselineEm
                         - static_cast<float>(lines.size() - 1) * lineAdvanceEm
                         + font->GetDescender();

    outMin = { xMin * scale, bottomEm * scale };
    outMax = { xMax * scale, topEm * scale };
}
