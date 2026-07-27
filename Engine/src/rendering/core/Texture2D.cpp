#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "engine/rendering/core/Texture2D.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/rendering/core/Sprite.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <vector>
#include "engine/utils/EngineUtils.hpp"

using namespace EngineUtils;

Texture2D::~Texture2D()
{
    Notify(DESTROYED_EVENT, GetID());
    if (texture_id)
        glDeleteTextures(1, &texture_id);
    if (normal_texture_id)
        glDeleteTextures(1, &normal_texture_id);
}


void Texture2D::ApplySettings() const
{
    glBindTexture(GL_TEXTURE_2D, texture_id);
    
    // Change GL_LINEAR_MIPMAP_LINEAR to just GL_LINEAR
    GLenum minFilter = (filter == TextureFilter::Linear) ? GL_LINEAR : GL_NEAREST;
    GLenum magFilter = (filter == TextureFilter::Linear) ? GL_LINEAR : GL_NEAREST;
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
    
    GLenum wrapMode = (wrap == TextureWrap::Repeat) ? GL_REPEAT : GL_CLAMP_TO_EDGE;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapMode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapMode);
}

void Texture2D::Awake(){
    stbi_set_flip_vertically_on_load(true);

    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
    if (!data)
    {
        std::cerr << "Failed to load texture: " << path << std::endl;
        return;
    }

    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    GLenum format = channels == 4 ? GL_RGBA : GL_RGB;

    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);

    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    ApplySettings();
    stbi_image_free(data);
}

void Texture2D::Deserialize(const YAML::Node &node){
    Resource::Deserialize(node);
    path = GetAssetPath(node["path"].as<std::string>());
    filter = node["filtering"].as<std::string>() == "linear" ? TextureFilter::Linear : TextureFilter::Nearest;
    wrap = node["wrap"].as<std::string>() == "clamp" ? TextureWrap::Clamp : TextureWrap::Repeat;

    // The generated image is never stored -- only the recipe. Textures authored
    // before this block existed simply keep Apply Normal off.
    if (const YAML::Node& n = node["normal"])
    {
        applyNormal    = n["apply"].as<bool>(false);
        normalStrength = n["strength"].as<float>(2.0f);
        normalBlur     = n["blur"].as<int>(1);
        normalInvertX  = n["invertX"].as<bool>(false);
        normalInvertY  = n["invertY"].as<bool>(false);
        heightSource   = n["source"].as<std::string>("luminance") == "alpha"
                            ? NormalHeightSource::Alpha : NormalHeightSource::Luminance;
        edgeFilter     = n["filter"].as<std::string>("sobel") == "scharr"
                            ? NormalEdgeFilter::Scharr : NormalEdgeFilter::Sobel;
    }
    // Regenerated on the first draw that needs it, when a GL context is current.
    normalDirty = applyNormal;
}

void Texture2D::RegisterSprite(const std::string& spriteId){
    if (std::find(sprite_ids.begin(), sprite_ids.end(), spriteId) == sprite_ids.end())
        sprite_ids.push_back(spriteId);
}

void Texture2D::UnregisterSprite(const std::string& spriteId){
    sprite_ids.erase(std::remove(sprite_ids.begin(), sprite_ids.end(), spriteId), sprite_ids.end());
}

YAML::Node Texture2D::Serialize(){
    YAML::Node node;
    node["type"] = GetTypeName();
    node["id"] = GetID();
    node["name"] = GetName();
    node["path"] = ToAssetRelative(path);
    node["filtering"] = filter == TextureFilter::Linear ? "linear" : "nearest";
    node["wrap"] = wrap == TextureWrap::Clamp ? "clamp" : "repeat";

    // Settings only. The normal map itself is in-memory and is rebuilt from
    // these on load -- deliberately no image data here and no file on disk.
    if (applyNormal)
    {
        YAML::Node normal;
        normal["apply"]    = applyNormal;
        normal["strength"] = normalStrength;
        normal["blur"]     = normalBlur;
        normal["invertX"]  = normalInvertX;
        normal["invertY"]  = normalInvertY;
        normal["source"]   = heightSource == NormalHeightSource::Alpha ? "alpha" : "luminance";
        normal["filter"]   = edgeFilter == NormalEdgeFilter::Scharr ? "scharr" : "sobel";
        node["normal"]     = normal;
    }

    // Sprites live inside their texture's meta file. Emit those recorded in load
    // order first, then any others pointing at this texture (so none are dropped).
    YAML::Node sprites(YAML::NodeType::Sequence);
    std::vector<std::string> emitted;
    auto emit = [&](Sprite* s){
        if (!s) return;
        sprites.push_back(s->Serialize());
        emitted.push_back(s->GetID());
    };
    for (const auto& sid : sprite_ids)
        emit(AssetManager::Get().GetSprite(sid));
    for (const auto& kv : AssetManager::Get().GetAllSprites()) {
        Sprite* s = kv.second;
        if (!s || s->GetTextureID() != GetID()) continue;
        if (std::find(emitted.begin(), emitted.end(), s->GetID()) != emitted.end()) continue;
        emit(s);
    }
    if (sprites.size() > 0) node["sprites"] = sprites;
    return node;
}


void Texture2D::Bind(unsigned int slot) const
{
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, texture_id);
}

void Texture2D::Unbind() const
{
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture2D::SetFilter(TextureFilter f) {
    filter = f;
    if (texture_id) ApplySettings();
    // The normal map mirrors the albedo's sampling so the two stay in register.
    normalDirty = normalDirty || applyNormal;
    Notify(FILTER_CHANGED_EVENT);
}

void Texture2D::SetWrap(TextureWrap w) {
    wrap = w;
    if (texture_id) ApplySettings();
    // Wrap also decides the gradient's edge behaviour, so the map has to be
    // regenerated, not just re-parameterized.
    normalDirty = normalDirty || applyNormal;
    Notify(WRAP_CHANGED_EVENT);
}

void Texture2D::Accept(IVisitor* v) { v->Visit(this); }

// ─────────────────────────────────────────────────────────────────────────────
// Normal map generation
//
// Height field -> optional blur -> gradient -> encoded tangent-space normal.
// The result lives only in `normal_texture_id`; nothing touches the filesystem.
// ─────────────────────────────────────────────────────────────────────────────
namespace {

    // Clamp or wrap a coordinate to match how the texture itself samples, so the
    // generated normals tile exactly when the texture is set to Repeat.
    inline int SampleCoord(int v, int size, TextureWrap wrap)
    {
        if (size <= 0) return 0;
        if (wrap == TextureWrap::Repeat)
        {
            v %= size;
            return v < 0 ? v + size : v;
        }
        return v < 0 ? 0 : (v >= size ? size - 1 : v);
    }

    // Separable box blur, applied `radius` wide in each axis. Softens the height
    // field so single-pixel colour noise doesn't become a field of sharp bumps.
    void BoxBlur(std::vector<float>& field, int w, int h, int radius, TextureWrap wrap)
    {
        if (radius <= 0 || w <= 0 || h <= 0) return;

        std::vector<float> tmp(field.size(), 0.0f);
        const float inv = 1.0f / static_cast<float>(radius * 2 + 1);

        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                float sum = 0.0f;
                for (int k = -radius; k <= radius; ++k)
                    sum += field[static_cast<std::size_t>(y) * w + SampleCoord(x + k, w, wrap)];
                tmp[static_cast<std::size_t>(y) * w + x] = sum * inv;
            }

        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                float sum = 0.0f;
                for (int k = -radius; k <= radius; ++k)
                    sum += tmp[static_cast<std::size_t>(SampleCoord(y + k, h, wrap)) * w + x];
                field[static_cast<std::size_t>(y) * w + x] = sum * inv;
            }
    }
}

void Texture2D::RebuildNormalMap()
{
    normalDirty = false;

    if (!applyNormal)
    {
        DestroyNormalMap();
        return;
    }

    // Awake() frees its pixels right after upload, so the source has to be
    // re-decoded here. The vertical flip MUST match Awake()'s, or the normal map
    // ends up mirrored against the albedo it is supposed to shade.
    stbi_set_flip_vertically_on_load(true);
    int w = 0, h = 0, sourceChannels = 0;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &sourceChannels, 4);
    if (!data || w <= 0 || h <= 0)
    {
        std::cerr << "Failed to load texture for normal map: " << path << std::endl;
        if (data) stbi_image_free(data);
        DestroyNormalMap();
        return;
    }

    const std::size_t pixelCount = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);
    std::vector<float> heightField(pixelCount, 0.0f);
    for (std::size_t i = 0; i < pixelCount; ++i)
    {
        const unsigned char* px = data + i * 4;
        if (heightSource == NormalHeightSource::Alpha)
        {
            heightField[i] = px[3] / 255.0f;
        }
        else
        {
            // Rec.601 luma: matches how the eye reads brightness, so painted
            // shading translates into believable relief.
            heightField[i] = (0.299f * px[0] + 0.587f * px[1] + 0.114f * px[2]) / 255.0f;
        }
    }
    stbi_image_free(data);

    BoxBlur(heightField, w, h, normalBlur, wrap);

    // 3x3 gradient kernels, normalized so `strength` means the same thing for both.
    float kx[9], ky[9];
    if (edgeFilter == NormalEdgeFilter::Scharr)
    {
        const float s = 1.0f / 32.0f;
        const float gx[9] = { -3, 0, 3, -10, 0, 10, -3, 0, 3 };
        const float gy[9] = { -3, -10, -3, 0, 0, 0, 3, 10, 3 };
        for (int i = 0; i < 9; ++i) { kx[i] = gx[i] * s; ky[i] = gy[i] * s; }
    }
    else
    {
        const float s = 1.0f / 8.0f;
        const float gx[9] = { -1, 0, 1, -2, 0, 2, -1, 0, 1 };
        const float gy[9] = { -1, -2, -1, 0, 0, 0, 1, 2, 1 };
        for (int i = 0; i < 9; ++i) { kx[i] = gx[i] * s; ky[i] = gy[i] * s; }
    }

    std::vector<unsigned char> normals(pixelCount * 3, 0);
    const float sx = normalInvertX ? -1.0f : 1.0f;
    const float sy = normalInvertY ? -1.0f : 1.0f;

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            float dx = 0.0f, dy = 0.0f;
            for (int j = -1; j <= 1; ++j)
            {
                const int yy = SampleCoord(y + j, h, wrap);
                for (int i = -1; i <= 1; ++i)
                {
                    const int xx = SampleCoord(x + i, w, wrap);
                    const float v = heightField[static_cast<std::size_t>(yy) * w + xx];
                    const int k = (j + 1) * 3 + (i + 1);
                    dx += v * kx[k];
                    dy += v * ky[k];
                }
            }

            // A surface rising toward +x tilts its normal toward -x, hence the
            // negation. Z is fixed at 1 and `strength` scales the slope, which is
            // the standard height-to-normal formulation.
            float nx = -dx * normalStrength * sx;
            float ny = -dy * normalStrength * sy;
            float nz = 1.0f;
            const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
            nx /= len; ny /= len; nz /= len;

            const std::size_t o = (static_cast<std::size_t>(y) * w + x) * 3;
            normals[o + 0] = static_cast<unsigned char>((nx * 0.5f + 0.5f) * 255.0f + 0.5f);
            normals[o + 1] = static_cast<unsigned char>((ny * 0.5f + 0.5f) * 255.0f + 0.5f);
            normals[o + 2] = static_cast<unsigned char>((nz * 0.5f + 0.5f) * 255.0f + 0.5f);
        }
    }

    if (!normal_texture_id)
        glGenTextures(1, &normal_texture_id);

    glBindTexture(GL_TEXTURE_2D, normal_texture_id);
    // Rows are 3 bytes wide and an odd width breaks the default 4-byte alignment.
    GLint prevAlignment = 4;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &prevAlignment);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, normals.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, prevAlignment);
    glGenerateMipmap(GL_TEXTURE_2D);

    // Mirror the albedo's sampling so the two stay in register, in particular
    // under Nearest filtering on pixel art.
    const GLenum minFilter = (filter == TextureFilter::Linear) ? GL_LINEAR : GL_NEAREST;
    const GLenum magFilter = minFilter;
    const GLenum wrapMode  = (wrap == TextureWrap::Repeat) ? GL_REPEAT : GL_CLAMP_TO_EDGE;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapMode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapMode);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture2D::DestroyNormalMap()
{
    if (normal_texture_id)
        glDeleteTextures(1, &normal_texture_id);
    normal_texture_id = 0;
}

void Texture2D::EnsureNormalMap()
{
    if (!normalDirty) return;
    RebuildNormalMap();
}

void Texture2D::SetApplyNormal(bool v)
{
    if (applyNormal == v) return;
    applyNormal = v;
    normalDirty = true;
    Notify(NORMAL_CHANGED_EVENT);
}

void Texture2D::SetNormalStrength(float v)
{
    v = std::clamp(v, 0.0f, 100.0f);
    if (normalStrength == v) return;
    normalStrength = v;
    normalDirty = true;
    Notify(NORMAL_CHANGED_EVENT);
}

void Texture2D::SetNormalBlur(int v)
{
    v = std::clamp(v, 0, 8);
    if (normalBlur == v) return;
    normalBlur = v;
    normalDirty = true;
    Notify(NORMAL_CHANGED_EVENT);
}

void Texture2D::SetNormalInvertX(bool v)
{
    if (normalInvertX == v) return;
    normalInvertX = v;
    normalDirty = true;
    Notify(NORMAL_CHANGED_EVENT);
}

void Texture2D::SetNormalInvertY(bool v)
{
    if (normalInvertY == v) return;
    normalInvertY = v;
    normalDirty = true;
    Notify(NORMAL_CHANGED_EVENT);
}

void Texture2D::SetNormalHeightSource(NormalHeightSource v)
{
    if (heightSource == v) return;
    heightSource = v;
    normalDirty = true;
    Notify(NORMAL_CHANGED_EVENT);
}

void Texture2D::SetNormalEdgeFilter(NormalEdgeFilter v)
{
    if (edgeFilter == v) return;
    edgeFilter = v;
    normalDirty = true;
    Notify(NORMAL_CHANGED_EVENT);
}
