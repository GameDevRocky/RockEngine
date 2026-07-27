#pragma once
#include <string>
#include <vector>
#include <glad/glad.h>
#include "engine/serialization/Serializable.hpp"
#include "engine/rendering/core/Resource.hpp"
#include <yaml-cpp/yaml.h>


enum class TextureFilter {
    Nearest,
    Linear
};

enum class TextureWrap {
    Repeat,
    Clamp
};

// Which channel of the source image is read as height when generating a normal
// map. Luminance suits art with painted shading; Alpha suits flat-coloured
// sprites whose silhouette carries the shape.
enum class NormalHeightSource {
    Luminance,
    Alpha
};

// Gradient operator used on the height field. Scharr has better rotational
// symmetry than Sobel and gives smoother diagonals; Sobel is crisper.
enum class NormalEdgeFilter {
    Sobel,
    Scharr
};

class Texture2D : public Resource {
public:
    static inline const Event FILTER_CHANGED_EVENT = Texture2D::CreateEvent();
    static inline const Event WRAP_CHANGED_EVENT   = Texture2D::CreateEvent();
    // Any change to the normal-map settings below. Fires the AssetManager
    // auto-save that persists them into the .texture meta.
    static inline const Event NORMAL_CHANGED_EVENT = Texture2D::CreateEvent();
    // Fired from the destructor (payload = id) so UI bound to this texture (e.g. the
    // sprite editor modal) can close itself when the texture is destroyed in memory.
    static inline const Event DESTROYED_EVENT      = Texture2D::CreateEvent();

    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;

    // Records a sprite id in load order so re-saving keeps sprites stable/ordered.
    void RegisterSprite(const std::string& spriteId);

    // Removes a sprite id from this texture's list (used when a sprite is deleted).
    void UnregisterSprite(const std::string& spriteId);

    void Awake() override;

    void Bind(unsigned int slot = 0) const;
    void Unbind() const;

    void ApplySettings() const;

    std::string GetTypeName() {return "Texture2D";};

    void Accept(IVisitor* v) override;

    int GetWidth() const { return width; }
    int GetHeight() const { return height; }

    std::string GetPath() const { return path; }

    TextureFilter GetFilter() const { return filter; }
    TextureWrap   GetWrap()   const { return wrap; }

    void SetFilter(TextureFilter f);
    void SetWrap(TextureWrap w);

    GLuint GetTextureID() const { return texture_id; }

    // ── Generated normal map ────────────────────────────────────────────────
    // "Apply Normal" derives a tangent-space normal map from this texture's own
    // grayscale and keeps it as a second GL texture. The generated image is
    // IN MEMORY ONLY -- nothing is ever written to disk. Only the settings below
    // persist (into the .texture meta), and the map is rebuilt from them on load.
    //
    // Rebuilds are deferred, not immediate: a setter only raises normalDirty, and
    // EnsureNormalMap() does the decode + upload from inside the render pass,
    // where a GL context is guaranteed current. Same "pull at render time" rule
    // the cameras follow.
    bool GetApplyNormal() const { return applyNormal; }
    void SetApplyNormal(bool v);

    float GetNormalStrength() const { return normalStrength; }
    void  SetNormalStrength(float v);

    int  GetNormalBlur() const { return normalBlur; }
    void SetNormalBlur(int v);

    bool GetNormalInvertX() const { return normalInvertX; }
    void SetNormalInvertX(bool v);

    bool GetNormalInvertY() const { return normalInvertY; }
    void SetNormalInvertY(bool v);

    NormalHeightSource GetNormalHeightSource() const { return heightSource; }
    void SetNormalHeightSource(NormalHeightSource v);

    NormalEdgeFilter GetNormalEdgeFilter() const { return edgeFilter; }
    void SetNormalEdgeFilter(NormalEdgeFilter v);

    // Rebuilds the normal texture if the settings changed since the last call.
    // Cheap no-op when clean. MUST be called with a current GL context.
    void EnsureNormalMap();

    // 0 when Apply Normal is off or generation failed.
    GLuint GetNormalTextureID() const { return normal_texture_id; }

    Texture2D() = default;
    ~Texture2D();

private:
    // Re-decodes the source file, builds the height field, runs the gradient
    // operator and uploads the result. Called only by EnsureNormalMap().
    void RebuildNormalMap();
    void DestroyNormalMap();

    GLuint texture_id = 0;
    int width = 0;
    int height = 0;
    int channels = 0;
    std::string path;
    std::vector<std::string> sprite_ids;
    TextureFilter filter = TextureFilter::Linear;
    TextureWrap wrap = TextureWrap::Clamp;

    GLuint normal_texture_id = 0;
    bool   normalDirty       = false;
    bool   applyNormal       = false;
    float  normalStrength    = 2.0f;
    int    normalBlur        = 1;
    bool   normalInvertX     = false;
    bool   normalInvertY     = false;
    NormalHeightSource heightSource = NormalHeightSource::Luminance;
    NormalEdgeFilter   edgeFilter   = NormalEdgeFilter::Sobel;
};
