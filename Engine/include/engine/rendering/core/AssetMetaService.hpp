#pragma once
#include <string>
#include <filesystem>

// Scans asset directories and generates missing meta (descriptor) files.
//
// Convention:
//   Source file          Meta file                     Asset type
//   ────────────────     ─────────────────────────     ─────────────
//   foo.png / .jpg       foo.png.texture               Texture2D
//   foo.glsl             foo.glsl.shader               Shader
//   foo.ttf / .otf       foo.ttf.font                  Font
//   foo.wav / .mp3 /     foo.wav.audio                 AudioClip
//     .ogg / .flac
//   foo.mat              (already a definition file)   Material
//
// Meta files are YAML and contain all fields that AssetManager / the
// individual Deserialize() methods expect, plus:
//   type: <TypeName>   (so LoadAssetFromFile can dispatch on type)
//
// IDs are deterministic: the sanitised filename stem, with a UUID suffix
// appended only if a collision is detected at generation time.
class AssetMetaService {
public:
    // Recursively scan rootDir; generate .texture, .shader, .font and .audio meta
    // files for any source file that does not already have one.
    // Safe to call on every startup – it skips files that already exist.
    static void ScanAndGenerate(const std::string& rootDir);

    // Generate the meta for a single source file, if it maps to a known asset
    // type (image → .texture, .glsl → .shader, .ttf/.otf → .font, audio → .audio)
    // and doesn't already have one. Used when importing one dropped file rather
    // than rescanning a whole tree.
    static void GenerateFor(const std::string& sourceFile);

    // Returns true if ext is a meta extension (.texture / .shader / .material / .font / .audio)
    static bool IsMetaExtension(const std::string& ext);

    // Returns the meta extension for a given source extension, or "" if unknown.
    //   ".png" → ".texture",  ".glsl" → ".shader",  ".ttf" → ".font",  ".wav" → ".audio"
    static std::string MetaExtensionFor(const std::string& sourceExt);

private:
    static void ProcessTexture(const std::filesystem::path& sourcePath);
    static void ProcessShader (const std::filesystem::path& glslPath);
    static void ProcessFont   (const std::filesystem::path& fontPath);
    static void ProcessAudio  (const std::filesystem::path& audioPath);

    // Convert an absolute path to a path relative to PROJECT_ROOT (forward slashes).
    static std::string ToProjectRelative(const std::filesystem::path& absPath);
};
