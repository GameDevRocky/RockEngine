#include "engine/rendering/core/Sprite.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/rendering/core/Texture2D.hpp"
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
