#include "engine/rendering/core/Sprite.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/rendering/core/Texture2D.hpp"
// Header only -- STB_IMAGE_IMPLEMENTATION lives in Texture2D.cpp.
#include "stb_image.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include "engine/utils/EngineUtils.hpp"

using namespace EngineUtils;
YAML::Node Sprite::Serialize(){
    YAML::Node node;
    node["id"] = GetID();
    node["name"] = GetName();
    node["uvMin"].push_back(uvMin.x); node["uvMin"].push_back(uvMin.y);
    node["uvMin"].SetStyle(YAML::EmitterStyle::Flow);
    node["uvMax"].push_back(uvMax.x); node["uvMax"].push_back(uvMax.y);
    node["uvMax"].SetStyle(YAML::EmitterStyle::Flow);
    node["pivot"].push_back(pivot.x); node["pivot"].push_back(pivot.y);
    node["pivot"].SetStyle(YAML::EmitterStyle::Flow);
    return node;
}

void Sprite::Deserialize(const YAML::Node& node){
    Resource::Deserialize(node);
    uvMin = {node["uvMin"][0].as<float>(), node["uvMin"][1].as<float>()};
    uvMax = {node["uvMax"][0].as<float>(), node["uvMax"][1].as<float>()};
    pivot = {node["pivot"][0].as<float>(), node["pivot"][1].as<float>()};
    if (node["texture_id"]) texture_id = node["texture_id"].as<std::string>();
    
}

Texture2D* Sprite::GetTexture(){
    Texture2D* tex = AssetManager::Get().GetTexture(texture_id);
    return tex;
}

void Sprite::SetTexture(std::string& id){
    Texture2D* tex = AssetManager::Get().GetTexture(id);
    if (tex) {
        texture_id = tex->GetID();
        Notify(TEXTURE_CHANGED_EVENT);
    }
}

void Sprite::Awake(){}

void Sprite::Accept(IVisitor* v) { v->Visit(this); }

glm::vec2 Sprite::GetPixelSize(){
    Texture2D* tex = GetTexture();
    if (!tex) return {0.0f, 0.0f};
    const float texWidth  = static_cast<float>(tex->GetWidth());
    const float texHeight = static_cast<float>(tex->GetHeight());

    return {
        (uvMax.x - uvMin.x) * texWidth,
        (uvMax.y - uvMin.y) * texHeight
    };
}

void Sprite::SetUVMin(glm::vec2 min)
{
    if (uvMin == min) return;
    uvMin = min;
    Notify(UV_MIN_CHANGED_EVENT);
}

void Sprite::SetUVMax(glm::vec2 max)
{
    if (uvMax == max) return;
    uvMax = max;
    Notify(UV_MAX_CHANGED_EVENT);
}


void Sprite::SetPivot(glm::vec2 p)
{
    if (pivot == p) return;
    pivot = p;
    Notify(PIVOT_CHANGED_EVENT);
}

const std::vector<glm::vec2>& Sprite::GetOpaqueOutline(float alphaThreshold)
{
    // Rebuild only when an input actually changed. Compared by value rather than
    // driven by the change events, because the texture's pixels can also arrive
    // late (Awake uploads after Deserialize), and a stale-cache bug here would
    // show up as a shadow silhouette that silently belongs to the wrong sprite.
    if (!outlineValid ||
        outlineUVMin != uvMin || outlineUVMax != uvMax ||
        outlineTextureId != texture_id || outlineThreshold != alphaThreshold)
    {
        RebuildOpaqueOutline(alphaThreshold);
    }
    return outlineCache;
}

void Sprite::RebuildOpaqueOutline(float alphaThreshold)
{
    outlineCache.clear();
    outlineValid     = true;
    outlineUVMin     = uvMin;
    outlineUVMax     = uvMax;
    outlineTextureId = texture_id;
    outlineThreshold = alphaThreshold;

    Texture2D* tex = GetTexture();
    if (!tex || tex->GetWidth() <= 0 || tex->GetHeight() <= 0) {
        // Texture not uploaded yet: leave the cache empty AND invalid so the next
        // call retries rather than baking in an empty silhouette forever.
        outlineValid = false;
        return;
    }

    // Texture2D frees its pixels right after upload, so the source has to be
    // re-decoded. Flip must match Awake()'s, which is what makes decoded row 0
    // the v=0 row -- i.e. this buffer is y-up in UV space, same as the quad.
    stbi_set_flip_vertically_on_load(true);
    int texW = 0, texH = 0, comps = 0;
    unsigned char* data = stbi_load(tex->GetPath().c_str(), &texW, &texH, &comps, 4);
    if (!data || texW <= 0 || texH <= 0) {
        if (data) stbi_image_free(data);
        outlineValid = false;
        return;
    }

    // The sprite's texel sub-rect within the (possibly atlas) texture.
    const int x0 = std::clamp(static_cast<int>(std::lround(uvMin.x * texW)), 0, texW);
    const int x1 = std::clamp(static_cast<int>(std::lround(uvMax.x * texW)), 0, texW);
    const int y0 = std::clamp(static_cast<int>(std::lround(uvMin.y * texH)), 0, texH);
    const int y1 = std::clamp(static_cast<int>(std::lround(uvMax.y * texH)), 0, texH);
    const int w = x1 - x0;
    const int h = y1 - y0;
    if (w <= 0 || h <= 0) { stbi_image_free(data); return; }

    const unsigned char cutoff = static_cast<unsigned char>(
        std::clamp(alphaThreshold, 0.0f, 1.0f) * 255.0f);

    // Opaque mask for the sub-rect. Anything outside counts as transparent, so a
    // sprite whose art runs to its own edge still gets a closed silhouette.
    std::vector<unsigned char> opaque(static_cast<std::size_t>(w) * h, 0);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            opaque[static_cast<std::size_t>(y) * w + x] =
                data[((static_cast<std::size_t>(y0 + y) * texW) + (x0 + x)) * 4 + 3] >= cutoff ? 1 : 0;
    stbi_image_free(data);

    auto isOpaque = [&](int x, int y) -> bool {
        if (x < 0 || y < 0 || x >= w || y >= h) return false;
        return opaque[static_cast<std::size_t>(y) * w + x] != 0;
    };

    // Centre the result on the quad: the shader builds the quad as
    // aPos * uSize with aPos in [-0.5, 0.5], so pixel (0,0) sits at -size/2.
    const glm::vec2 half(w * 0.5f, h * 0.5f);
    auto emit = [&](float ax, float ay, float bx, float by) {
        outlineCache.push_back(glm::vec2(ax, ay) - half);
        outlineCache.push_back(glm::vec2(bx, by) - half);
    };

    // Vertical boundaries: for each pixel column edge, merge the consecutive rows
    // where opacity flips across it into one segment. Merging matters -- a raw
    // per-pixel edge list is ~4x larger and every extra segment is re-rasterized
    // once per quadrant per shadow-casting light.
    for (int x = 0; x <= w; ++x) {
        int runStart = -1;
        for (int y = 0; y <= h; ++y) {
            const bool edge = (y < h) && (isOpaque(x - 1, y) != isOpaque(x, y));
            if (edge && runStart < 0) runStart = y;
            else if (!edge && runStart >= 0) {
                emit(static_cast<float>(x), static_cast<float>(runStart),
                     static_cast<float>(x), static_cast<float>(y));
                runStart = -1;
            }
        }
    }

    // Horizontal boundaries, same merge along x.
    for (int y = 0; y <= h; ++y) {
        int runStart = -1;
        for (int x = 0; x <= w; ++x) {
            const bool edge = (x < w) && (isOpaque(x, y - 1) != isOpaque(x, y));
            if (edge && runStart < 0) runStart = x;
            else if (!edge && runStart >= 0) {
                emit(static_cast<float>(runStart), static_cast<float>(y),
                     static_cast<float>(x), static_cast<float>(y));
                runStart = -1;
            }
        }
    }
}
