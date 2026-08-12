#include "engine/rendering/core/Font.hpp"

#include <algorithm>

#include "engine/debug/Console.hpp"
#include "engine/utils/EngineUtils.hpp"

using namespace EngineUtils;

Font::~Font() {
    Notify(DESTROYED_EVENT, GetID());
    DestroyAtlas();
}

void Font::DestroyAtlas() {
    if (atlas_texture_id) {
        glDeleteTextures(1, &atlas_texture_id);
        atlas_texture_id = 0;
    }
    atlasWidth = atlasHeight = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
YAML::Node Font::Serialize() {
    YAML::Node node;
    node["type"] = GetTypeName();
    node["id"]   = GetID();
    node["name"] = GetName();
    node["path"] = ToAssetRelative(source_path);

    YAML::Node bake;
    bake["em_size"]  = emSize;
    bake["px_range"] = pxRange;
    // Only written when non-default: an empty charset means printable ASCII, and
    // spelling all 95 characters into every meta file would be noise.
    if (!charset.empty()) bake["charset"] = charset;
    node["bake"] = bake;

    return node;
}

void Font::Deserialize(const YAML::Node& node) {
    Resource::Deserialize(node);
    source_path = GetAssetPath(node["path"].as<std::string>(""));

    if (const YAML::Node& bake = node["bake"]) {
        emSize  = bake["em_size"].as<int>(48);
        pxRange = bake["px_range"].as<float>(4.0f);
        charset = bake["charset"].as<std::string>("");
    }
    emSize  = std::clamp(emSize, 8, 256);
    pxRange = std::clamp(pxRange, 1.0f, 32.0f);

    dirty = true;
    bakeFailed = false;
}

void Font::Awake() {
    // Deliberately no GL and no bake here. A font nobody references should cost
    // nothing, so the work is deferred to the first EnsureUploaded() from the
    // render path -- which is also the first point a GL context is guaranteed.
    dirty = true;
}

// ─────────────────────────────────────────────────────────────────────────────
void Font::EnsureUploaded() {
    if (!dirty || bakeFailed) return;
    Rebuild();
}

void Font::Rebuild() {
    dirty = false;

    if (source_path.empty()) {
        bakeFailed = true;
        Console::Alert("Font '" + GetName() + "' has no source path");
        return;
    }

    FontBakeSettings settings;
    settings.emSize  = emSize;
    settings.pxRange = pxRange;
    settings.charset = charset;

    // No disk cache by design -- see FontAtlasBaker::Bake. This runs once per
    // session per font, and costs ~265 ms optimized but ~2 s in a Debug build.
    // Announced because that is a visible stall and a user watching the editor
    // hitch deserves to know what caused it.
    Console::Alert("Baking MSDF atlas for font '" + GetName() + "'...");
    BakedAtlas atlas = FontAtlasBaker::Bake(source_path, settings);
    if (!atlas.ok) {
        bakeFailed = true;
        Console::Alert("Font bake failed for '" + GetName() + "': " + atlas.error);
        return;
    }

    glyphs     = std::move(atlas.glyphs);
    ascender   = atlas.ascender;
    descender  = atlas.descender;
    lineHeight = atlas.lineHeight;

    kerning.clear();
    kerning.reserve(atlas.kerning.size());
    for (const auto& kv : atlas.kerning) kerning.emplace(kv.first, kv.second);

    DestroyAtlas();
    atlasWidth  = atlas.width;
    atlasHeight = atlas.height;

    glGenTextures(1, &atlas_texture_id);
    glBindTexture(GL_TEXTURE_2D, atlas_texture_id);

    // Rows are tightly packed RGB8, so a width that isn't a multiple of 4 would be
    // misread under GL's default 4-byte row alignment -- shearing the atlas
    // diagonally. Set it explicitly and put it back afterwards.
    GLint prevAlignment = 4;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &prevAlignment);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, atlasWidth, atlasHeight, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, atlas.pixels.data());

    glPixelStorei(GL_UNPACK_ALIGNMENT, prevAlignment);

    // NO MIPMAPS. Averaging neighbouring texels averages three independent
    // distance fields, and the median of the result is not the median of the
    // originals -- glyphs dissolve into grey mush as they shrink. The shader's
    // screenPxRange() handles minification analytically instead.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // Clamp, so a tap at the very edge of the outermost glyph cannot wrap around
    // and pick up the field of a glyph on the opposite side of the atlas.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);

    ++atlasGeneration;
    Notify(ATLAS_REBUILT_EVENT, GetID());
}

// ─────────────────────────────────────────────────────────────────────────────
const BakedGlyph* Font::GetGlyph(std::uint32_t codepoint) const {
    auto it = std::lower_bound(glyphs.begin(), glyphs.end(), codepoint,
        [](const BakedGlyph& g, std::uint32_t cp) { return g.codepoint < cp; });
    if (it == glyphs.end() || it->codepoint != codepoint) return nullptr;
    return &*it;
}

float Font::GetKerning(std::uint32_t left, std::uint32_t right) const {
    const std::uint64_t key = (static_cast<std::uint64_t>(left) << 32) | right;
    auto it = kerning.find(key);
    return it != kerning.end() ? it->second : 0.0f;
}

// ─────────────────────────────────────────────────────────────────────────────
void Font::SetEmSize(int v) {
    v = std::clamp(v, 8, 256);
    if (v == emSize) return;
    emSize = v;
    dirty = true;
    bakeFailed = false;   // new settings deserve a fresh attempt
    Notify(BAKE_SETTINGS_CHANGED_EVENT);
}

void Font::SetPxRange(float v) {
    v = std::clamp(v, 1.0f, 32.0f);
    if (v == pxRange) return;
    pxRange = v;
    dirty = true;
    bakeFailed = false;
    Notify(BAKE_SETTINGS_CHANGED_EVENT);
}

void Font::SetCharset(const std::string& v) {
    if (v == charset) return;
    charset = v;
    dirty = true;
    bakeFailed = false;
    Notify(BAKE_SETTINGS_CHANGED_EVENT);
}

void Font::Accept(IVisitor* v) { v->Visit(this); }
