#include "engine/rendering/core/Sprite.hpp"
#include "engine/rendering/core/SharedResources.hpp"
#include <iostream>

void Sprite::Deserialize(const YAML::Node& node){
    Serializable::Deserialize(node);
    name = node["name"].as<std::string>();
    uvMin = {node["uvMin"][0].as<float>(), node["uvMin"][1].as<float>()};
    uvMax = {node["uvMax"][0].as<float>(), node["uvMax"][1].as<float>()};
    pivot = {node["pivot"][0].as<float>(), node["pivot"][1].as<float>()};
    texture_id = node["texture_id"].as<std::string>();
    
}

Texture2D* Sprite::GetTexture(){
    Texture2D* tex = SharedResources::Get().GetTexture(texture_id);
    return tex;
}

void Sprite::SetTexture(std::string& id){
    Texture2D* tex = SharedResources::Get().GetTexture(id);
    if (tex) texture_id = tex->GetID();

}

void Sprite::PostDeserialize()
{
    Texture2D* tex = GetTexture();
    if (!tex)
        return;

    const float texWidth  = static_cast<float>(tex->GetWidth());
    const float texHeight = static_cast<float>(tex->GetHeight());

    pixelSize = {
        (uvMax.x - uvMin.x) * texWidth,
        (uvMax.y - uvMin.y) * texHeight
    };
}


void Sprite::SetUVMin(glm::vec2 min)
{
    uvMin = min;
}

void Sprite::SetUVMax(glm::vec2 max)
{
    uvMax = max;
}


void Sprite::SetPivot(glm::vec2 p)
{
    pivot = p;
}
