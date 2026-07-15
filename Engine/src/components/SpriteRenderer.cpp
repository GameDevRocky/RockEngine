#include "engine/components/SpriteRenderer.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/debug/Console.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "engine/components/Transform.hpp"
#include "iostream"
using namespace EngineUtils::RenderUtils;

YAML::Node SpriteRenderer::Serialize()
{
    YAML::Node node = Component::Serialize();
    node["material_id"] = material_id;
    node["sprite_id"] = sprite_id;
    node["color"][0] = color.r;
    node["color"][1] = color.g;
    node["color"][2] = color.b;
    node["color"][3] = color.a;
    node["color"].SetStyle(YAML::EmitterStyle::Flow);
    node["uvOffset"][0] = uvOffset.x;
    node["uvOffset"][1] = uvOffset.y;
    node["uvOffset"].SetStyle(YAML::EmitterStyle::Flow);
    node["uvScale"][0] = uvScale.x;
    node["uvScale"][1] = uvScale.y;
    node["uvScale"].SetStyle(YAML::EmitterStyle::Flow);
    node["sortingOrder"] = sortingOrder;
    node["sortingLayer"] = sortingLayer;
    node["flipX"] = flipX;
    node["flipY"] = flipY;
    node["visible"] = visible;
    return node;
}

void SpriteRenderer::Deserialize(const YAML::Node& node)
{
    Component::Deserialize(node);
    material_id = node["material_id"].as<std::string>();
    sprite_id = node["sprite_id"].as<std::string>();
    color = glm::vec4( node["color"][0].as<float>(), node["color"][1].as<float>(), node["color"][2].as<float>(), node["color"][3].as<float>() );
    uvOffset = glm::vec2( node["uvOffset"][0].as<float>(), node["uvOffset"][1].as<float>());
    uvScale = glm::vec2( node["uvScale"][0].as<float>(), node["uvScale"][1].as<float>());
    flipX = node["flipX"].as<bool>();
    flipY = node["flipY"].as<bool>();
    visible = node["visible"].as<bool>();
    sortingOrder = node["sortingOrder"].as<int>();
    sortingLayer = node["sortingLayer"].as<std::string>("Default");
    state = State::Loaded;
}

Material* SpriteRenderer::GetMaterial(){
    Material* mat = AssetManager::Get().GetMaterial(material_id);
    if (!mat){
        if (!material_id.empty())
            Console::Alert("Unable to load " + GetGameObject()->GetName() + "'s material");
        return nullptr;
    }
    return mat;
}

void SpriteRenderer::SetMaterial(std::string& id){
    Material* mat = AssetManager::Get().GetMaterial(id);
    if (!mat){
        Console::Alert("Unable to load " + GetGameObject()->GetName() + "'s Material");
        return;
    }
    material_id = mat->GetID();
    this->Notify(SpriteRenderer::MATERIAL_CHANGED_EVENT);
    this->Notify(SpriteRenderer::CHANGED_EVENT);
}


Sprite* SpriteRenderer::GetSprite(){
    Sprite* sprite = AssetManager::Get().GetSprite(sprite_id);
    if (!sprite){
        return nullptr;
    }
    return sprite;
}

void SpriteRenderer::SetSprite(std::string& id){
    Sprite* sprite = AssetManager::Get().GetSprite(id);
    if (!sprite){
        Console::Alert("Unable to load " + GetGameObject()->GetName() + "'s Sprite");
        return;
    }
    sprite_id = sprite->GetID();
    this->Notify(SpriteRenderer::SPRITE_CHANGED_EVENT);
    this->Notify(SpriteRenderer::CHANGED_EVENT);
}

void SpriteRenderer::SetVisible(bool& val){
    visible = val;
    this->Notify(SpriteRenderer::VISIBILITY_CHANGED_EVENT);
    this->Notify(SpriteRenderer::CHANGED_EVENT);

}

void SpriteRenderer::SetSortingLayer(const std::string& layer){
    if (sortingLayer == layer) return;
    sortingLayer = layer;
    this->Notify(SpriteRenderer::SORTING_LAYER_CHANGED_EVENT);
    this->Notify(SpriteRenderer::CHANGED_EVENT);
}

void SpriteRenderer::SetSortingOrder(int order){
    if (sortingOrder == order) return;
    sortingOrder = order;
    this->Notify(SpriteRenderer::SORTING_ORDER_CHANGED_EVENT);
    this->Notify(SpriteRenderer::CHANGED_EVENT);
}

void SpriteRenderer::SetUniformOverride(const std::string& name, const UniformValue& value)
{
    uniformOverrides[name] = value;
}

void SpriteRenderer::RemoveUniformOverride(const std::string& name)
{
    uniformOverrides.erase(name);
}

void SpriteRenderer::OverrideUniforms()
{
    Material* mat = GetMaterial();
    Sprite* sprite = GetSprite();

    if (!sprite || !mat)
        return;

    Texture2D* tex = sprite->GetTexture();
    Shader* shader = mat->GetShader();

    if (!shader || !tex)
        return;

    tex->Bind(0);
    shader->SetTexture("uTexture", 0);

    glm::vec2 uvScale = sprite->GetUVMax() - sprite->GetUVMin();
    glm::vec2 uvOffset = sprite->GetUVMin();

    if (flipX) {
        uvScale.x *= -1.0f;
        uvOffset.x = sprite->GetUVMax().x;
    }

    if (flipY) {
        uvScale.y *= -1.0f;
        uvOffset.y = sprite->GetUVMax().y;
    }

    uvScale *= this->uvScale;   
    uvOffset += this->uvOffset; 

    shader->SetVec2("uUVScale", uvScale);
    shader->SetVec2("uUVOffset", uvOffset);

    const glm::vec2 pixelSize = sprite->GetPixelSize();

    glm::vec2 worldSize = PixelsToWorld(pixelSize);


    shader->SetVec2("uSize", worldSize);
    shader->SetVec2("uPivot", sprite->GetPivot());
    shader->SetVec4("uColor", color);

    int textureSlot = 1;
    for (const auto& [uName, uValue] : uniformOverrides) {
        std::visit([&](const auto& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, float>)
                shader->SetFloat(uName, v);
            else if constexpr (std::is_same_v<T, glm::vec2>)
                shader->SetVec2(uName, v);
            else if constexpr (std::is_same_v<T, glm::vec3>)
                shader->SetVec3(uName, v);
            else if constexpr (std::is_same_v<T, glm::vec4>)
                shader->SetVec4(uName, v);
            else if constexpr (std::is_same_v<T, std::string>) {
                Texture2D* tex = AssetManager::Get().GetTexture(v);
                if (tex) {
                    tex->Bind(textureSlot);
                    shader->SetTexture(uName, textureSlot++);
                }
            }
        }, uValue);
    }
}

void SpriteRenderer::Accept(IVisitor* v) {
    
    v->Visit(this); 
}

SpriteRenderer* SpriteRenderer::Copy()
{
    SpriteRenderer* copy = new SpriteRenderer();
    copy->id = id;
    copy->enabled = enabled;
    copy->gameobject_id = gameobject_id;
    copy->material_id = material_id;
    copy->sprite_id = sprite_id;
    copy->uvOffset = uvOffset;
    copy->uvScale = uvScale;
    copy->color = color;
    copy->sortingOrder = sortingOrder;
    copy->flipX = flipX;
    copy->flipY = flipY;
    copy->visible = visible;
    copy->uniformOverrides = uniformOverrides;
    return copy;


}
