#include "engine/rendering/core/Font.hpp"

#include <algorithm>

#include "engine/debug/Console.hpp"
#include "engine/jobs/JobSystem.hpp"
#include "engine/rendering/core/AssetManager.hpp"
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
// Three states, checked in this order, called every frame from the draw path
// (ScenePass) -- which is the only place a current GL context is guaranteed.
//
//   1. a bake came back -> install it (this is the only GL in the whole flow)
//   2. clean, poisoned, or already baking -> nothing to do
//   3. otherwise -> hand the bake to a worker and return immediately
//
// The draw path already gates on IsReady(), so a font mid-bake simply emits no
// draw call and retries next frame. That pre-existing gate is why the bake could
// be made asynchronous without touching ScenePass at all.
//
// Changing a bake setting mid-flight self-corrects at the cost of one wasted
// bake: the setter raises `dirty` but bakeInFlight blocks a second submit, so
// the stale result installs, clears the latch, and the still-set `dirty` submits
// a fresh bake on the following frame. Deliberately not optimized -- the
// alternative is tracking which settings a running bake was started with.
void Font::EnsureUploaded() {
    if (pendingAtlas) {
        InstallAtlas(*pendingAtlas);
        pendingAtlas.reset();
        bakeInFlight = false;
        return;
    }
    if (!dirty || bakeFailed || bakeInFlight) return;

    // Cleared before the work starts, exactly as the synchronous version did, so
    // a failure can't loop.
    dirty = false;

    if (source_path.empty()) {
        bakeFailed = true;
        Console::Alert("Font '" + GetName() + "' has no source path");
        return;
    }

    bakeInFlight = true;
    SubmitBake();
}

void Font::SubmitBake() {
    FontBakeSettings settings;
    settings.emSize  = emSize;
    settings.pxRange = pxRange;
    settings.charset = charset;

    // Captured by value: the worker must not read a single member of `this`.
    const std::string path = source_path;
    const std::string name = GetName();
    // The id, never a Font* -- the font can be deleted while the bake is out
    // (AssetManager::RemoveAsset), and the completion has to be able to tell.
    const std::string fontId = GetID();

    // Shared between the two halves: written only by the worker, read only by
    // the main step, with the job system's mutex providing the happens-before.
    auto result = std::make_shared<BakedAtlas>();

    JobDesc desc;
    desc.title = "Baking font atlas: " + name;
    // Modal. A bake is a visible stall the user deserves an explanation for --
    // the same reasoning behind the Console::Alert this replaced. The 150ms
    // grace delay in the overlay means a fast bake still never flashes a card.
    desc.modal = true;

    desc.worker = [result, path, settings](JobProgressSink& sink) {
        sink.Report(-1.0f, "Rasterizing glyphs");
        // Pure CPU and documented never to throw. This is the entire ~265 ms
        // (~2 s in Debug) that used to sit on the GUI thread.
        *result = FontAtlasBaker::Bake(path, settings);
        if (!result->ok) sink.Fail(result->error);
    };

    desc.mainStep = [result, fontId](JobProgressSink&) -> bool {
        // Main thread, but NOT inside paintGL -- no GL here. Park the result and
        // let the next draw-path EnsureUploaded do the upload.
        if (Font* font = AssetManager::Get().GetFont(fontId))
            font->pendingAtlas = std::make_unique<BakedAtlas>(std::move(*result));
        // else: the font was deleted mid-bake. Dropping the result is the whole
        // reason this captures an id instead of a pointer.
        return false;
    };

    desc.onFinished = [fontId, name](bool ok, const std::string& error) {
        if (ok) return;
        Console::Alert("Font bake failed for '" + name + "': " + error);
        // Re-latch the poison on the live object, so a broken font doesn't
        // re-submit a doomed bake on every frame forever.
        if (Font* font = AssetManager::Get().GetFont(fontId)) {
            font->bakeFailed   = true;
            font->bakeInFlight = false;
        }
    };

    JobSystem::Get().Submit(std::move(desc));
}

void Font::InstallAtlas(BakedAtlas& atlas) {
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
