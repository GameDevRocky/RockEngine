#pragma once

#include <atomic>
#include <string>
#include <random>
#include <sstream>
#include <string_view>
#include <glm/glm.hpp>

// 1. Define the cross-compiler macro at the top
#if defined(_MSC_VER)
#define ROCK_FUNC_SIG __FUNCSIG__
#else
#define ROCK_FUNC_SIG __PRETTY_FUNCTION__
#endif

namespace EngineUtils {
    // Directory containing the running executable (resolved via OS APIs).
    std::string ExecutableDir();

    // Resolve a path relative to the asset root. In a bundled/distributed build the
    // root is the folder next to the executable (where Domain/ is copied); in a local
    // dev build it falls back to the compiled-in PROJECT_ROOT. Decided once, cached.
    std::string GetAssetPath(const std::string& relativePath);

    // The resolved asset root (folder next to the executable for bundled builds,
    // else the compiled-in PROJECT_ROOT). Decided once, cached.
    std::string GetAssetRoot();

    // Inverse of GetAssetPath: convert an absolute path back to a forward-slash
    // path relative to the asset root. Returns absPath unchanged if outside the root.
    std::string ToAssetRelative(const std::string& absPath);

    namespace RenderUtils {
        // 1 world unit == 1 pixel. A texture/sprite's pixel size IS its world-unit
        // size, so these render conversions are identity. They're kept (rather than
        // deleted) so call sites still read as an explicit pixel<->world crossing.
        inline float     PixelsToWorld(float pixels)             { return pixels; }
        inline glm::vec2 PixelsToWorld(const glm::vec2& pixels)  { return pixels; }
        inline float     WorldToPixels(float units)              { return units; }
        inline glm::vec2 WorldToPixels(const glm::vec2& units)   { return units; }

        // Physics ONLY. Box2D solves in meters; world units (pixels) / PixelsPerMeter
        // = meters. This is the single home of the physics scale -- anything crossing
        // the Box2D boundary goes through these, everything visual stays 1:1 pixels.
        constexpr float  PixelsPerMeter = 32.0f;
        inline float     PixelsToMeters(float pixels)            { return pixels / PixelsPerMeter; }
        inline glm::vec2 PixelsToMeters(const glm::vec2& pixels) { return pixels / PixelsPerMeter; }
        inline float     MetersToPixels(float meters)            { return meters * PixelsPerMeter; }
        inline glm::vec2 MetersToPixels(const glm::vec2& meters) { return meters * PixelsPerMeter; }

        // NOT the live grid cell size any more -- that is GridSettings::GetCellSize(),
        // which the scene view's toolbar drives at runtime, and which the grid pass
        // and the transform gizmo both read.
        //
        // What survives here is the default those settings start at, plus a stable
        // reference unit for things that want "about one cell" without following
        // the user's choice: SceneViewGui's frame-selection zoom floor uses it so
        // that setting a huge grid cell doesn't make "frame selected" zoom to orbit.
        constexpr float  GridCellPixels = 32.0f;

        // Reference vertical resolution a new game Camera's default orthoSize is
        // derived from (orthoSize default = ReferenceHeight / 2).
        constexpr float  ReferenceHeight = 1080.0f;
    }

    // Texture units the ENGINE binds behind the material system's back.
    //
    // Material::ApplyUniforms allocates slots from 0 upward over its sampler
    // uniforms, and SpriteRenderer's per-instance texture overrides continue from
    // 1 -- both counting up from the bottom. These sit high and fixed so an
    // engine-managed texture can never be stomped by a material that happens to
    // declare one more sampler than it did yesterday.
    namespace TextureSlots {
        constexpr int NormalMap   = 12;   // generated from the sprite texture's grayscale
        constexpr int ShadowAtlas = 13;   // per-light polar depth maps
        constexpr int FontAtlas   = 14;   // a Font's baked MSDF glyph atlas
    }

    namespace MathUtils {
        constexpr float RAD_2_DEG = 180.0f / 3.14159265359f;
        constexpr float DEG_2_RAD = 3.14159265359f / 180.0f;
        constexpr float PI = 3.14159265359f;
    }

    std::string GenerateUUID();
    std::string ReadShader(const std::string& path);

    // What vertex data a shader expects to be fed. Declared in the .glsl via
    // `#pragma domain <name>` before the stage markers; absent means Sprite, so
    // every shader written before this existed keeps its meaning.
    //
    //   Sprite -- ScenePass's unit quad: aPos in [-0.5,0.5] scaled by uSize and
    //             shifted by uPivot, UVs remapped through uUVScale/uUVOffset.
    //   Text   -- a glyph mesh: aPos already positioned in text-local world
    //             units, aUV already the glyph's rect in an MSDF atlas.
    //
    // Purely advisory. TextRenderer neutralizes the sprite quad's transform
    // uniforms so a Sprite-domain material still positions text correctly; the
    // domain exists so the editor can filter the material picker and warn on a
    // mismatch rather than leaving the user to wonder why their text is rainbow
    // noise (a sprite shader samples the atlas as colour instead of decoding it).
    enum class ShaderDomain { Sprite, Text };

    struct ShaderSource {
        std::string vertex;
        std::string fragment;
        ShaderDomain domain = ShaderDomain::Sprite;
    };
    // Parse a combined .glsl file split by #pragma vertex / #pragma fragment markers.
    ShaderSource ParseShaderSource(const std::string& path);

    // 2. ONLY ONE VERSION OF THIS FUNCTION
    template<typename T>
    constexpr std::string_view TypeName() {
        constexpr std::string_view n = ROCK_FUNC_SIG;
#if defined(__clang__) || defined(__GNUC__)
        auto start = n.find("T = ") + 4;
        auto end = n.find_last_of(']');
        if (end == std::string_view::npos) end = n.find(';', start);
        return n.substr(start, end - start);
#elif defined(_MSC_VER)
        // 1. Find the last opening bracket to ignore the return type/namespace
        auto start = n.find_last_of('<') + 1;
        auto end = n.find_last_of('>');
        std::string_view raw = n.substr(start, end - start);

        // 2. Strip MSVC keywords if they exist at the start
        if (raw.starts_with("class ")) return raw.substr(6);
        if (raw.starts_with("struct ")) return raw.substr(7);

        return raw;
#endif
    }
}