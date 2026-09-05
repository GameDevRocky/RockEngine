#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <glad/glad.h>
#include <yaml-cpp/yaml.h>

#include "engine/rendering/core/Resource.hpp"
#include "engine/rendering/core/FontAtlasBaker.hpp"

// A typeface plus its baked MSDF atlas, as an engine asset.
//
// Backed by a `.font` YAML meta next to the source .ttf/.otf, which stores only
// the *recipe* -- source path, em size, distance range, charset. The atlas is
// never persisted: it is rebuilt from the source font once per session, on first
// use. That keeps the project tree free of large binary build products at the
// cost of a bake on each launch (~265 ms optimized, ~2 s in Debug), which is why
// EnsureUploaded is lazy rather than eager -- a font nobody draws costs nothing.
//
// Deliberately NOT a Texture2D. Texture2D decodes an image file through stb and
// generates mipmaps; an MSDF atlas comes from memory and must have mipmaps OFF
// (mip averaging destroys the median reconstruction and turns text to mush at
// distance), so sharing the type would mean special-casing it throughout.
class Font : public Resource {
public:
    // Atlas or glyph table changed -- anything holding a built text mesh must
    // rebuild it, because the UVs it baked in are now stale.
    static inline const Event ATLAS_REBUILT_EVENT   = Font::CreateEvent();
    // A bake setting changed. Drives AssetManager's auto-save of the .font meta.
    static inline const Event BAKE_SETTINGS_CHANGED_EVENT = Font::CreateEvent();
    // Fired from the destructor (payload = id) so UI bound to this font can drop it.
    static inline const Event DESTROYED_EVENT       = Font::CreateEvent();

    Font() = default;
    ~Font();

    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;
    void Awake() override;

    std::string GetTypeName() const override { return "Font"; }
    void Accept(IVisitor* v) override;

    // Bake and upload if needed. MUST be called with a current GL context; cheap
    // no-op once clean. Same "pull at render time" rule the cameras and
    // Texture2D's generated normal map follow -- the inspector setter that
    // dirties a font runs on the Qt side, where no context is current.
    //
    // The first call for a given font stalls for the length of a bake. That is
    // why it is called from the draw path rather than at asset load: it keeps
    // unused fonts free, and puts the cost at the moment text first appears.
    void EnsureUploaded();

    GLuint GetAtlasTextureID() const { return atlas_texture_id; }
    int    GetAtlasWidth()  const { return atlasWidth; }
    int    GetAtlasHeight() const { return atlasHeight; }
    bool   IsReady() const { return atlas_texture_id != 0; }

    // Bumped on every successful rebuild. Anything caching geometry derived from
    // this font (i.e. FontManager's glyph meshes, which bake UVs in) folds this
    // into its staleness check -- re-baking at a different em size repacks the
    // atlas and moves every glyph, so cached UVs silently point at the wrong
    // characters without it.
    std::uint32_t GetAtlasGeneration() const { return atlasGeneration; }

    // nullptr when the codepoint is not in the baked charset. Callers should fall
    // back to '?' rather than skipping, so a missing glyph is visible not silent.
    const BakedGlyph* GetGlyph(std::uint32_t codepoint) const;

    // Additional pen advance between two glyphs, em. 0 when the pair isn't kerned.
    float GetKerning(std::uint32_t left, std::uint32_t right) const;

    // Vertical metrics, em. descender is negative.
    float GetAscender()   const { return ascender; }
    float GetDescender()  const { return descender; }
    float GetLineHeight() const { return lineHeight; }

    // The shader needs this to turn atlas distances into screen pixels. It is a
    // property of the bake, not of any material -- see the reserved-uniform list
    // in Material.cpp.
    float GetPxRange() const { return pxRange; }

    const std::string& GetSourcePath() const { return source_path; }

    int  GetEmSize() const { return emSize; }
    void SetEmSize(int v);

    void SetPxRange(float v);

    const std::string& GetCharset() const { return charset; }
    void SetCharset(const std::string& v);

private:
    // Hand the bake to a worker thread. Pure CPU, so it needs nothing from here
    // but the recipe; the result comes back as a parked BakedAtlas.
    void SubmitBake();
    // Install a finished bake: glyph table, metrics, and the GL upload. MUST be
    // called with a current context -- only EnsureUploaded does.
    void InstallAtlas(BakedAtlas& atlas);
    void DestroyAtlas();

    std::string source_path;    // absolute path to the .ttf/.otf

    // ── Bake recipe (serialized) ────────────────────────────────────────────
    int         emSize  = 48;
    float       pxRange = 4.0f;
    std::string charset;        // empty == printable ASCII

    // ── Baked product (never serialized) ────────────────────────────────────
    GLuint atlas_texture_id = 0;
    int    atlasWidth = 0, atlasHeight = 0;
    std::uint32_t atlasGeneration = 0;
    float  ascender = 0.0f, descender = 0.0f, lineHeight = 1.2f;

    // Sorted by codepoint so lookup is a binary search; a face is baked once and
    // read every frame, so the build cost is irrelevant and the cache locality
    // beats a hash map at these sizes.
    std::vector<BakedGlyph> glyphs;
    std::unordered_map<std::uint64_t, float> kerning;

    bool dirty = true;
    // Set once a bake has failed, so a broken font file doesn't retry (and
    // re-log) on every single frame.
    bool bakeFailed = false;

    // A bake is out on a worker thread right now. Without this, EnsureUploaded
    // -- which the draw loop calls EVERY FRAME -- would submit a fresh ~265 ms
    // bake on each of the ~16 frames the first one is still running. Same role
    // as bakeFailed: a latch that stops the draw path re-triggering work that is
    // already accounted for.
    bool bakeInFlight = false;

    // A finished bake waiting for a frame with a current GL context. The job
    // system's main-thread step parks it here (it runs outside paintGL, where no
    // context is current); the next EnsureUploaded from the draw path uploads it.
    std::unique_ptr<BakedAtlas> pendingAtlas;
};
