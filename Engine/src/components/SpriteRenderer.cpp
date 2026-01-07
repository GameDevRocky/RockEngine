#include "engine/components/SpriteRenderer.hpp"
#include "engine/rendering/core/SharedResources.hpp"
#include "engine/debug/Console.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "engine/components/Transform.hpp"
#include "iostream"
using namespace EngineUtils::RenderUtils;

YAML::Node SpriteRenderer::Serialize()
{
    YAML::Node node;
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
}

void SpriteRenderer::PostDeserialize()
{
    
}

Material* SpriteRenderer::GetMaterial(){
    Material* mat = SharedResources::Get().GetMaterial(material_id);
    if (!mat){
        Console::Alert("Unable to load " + GetGameObject()->GetName() + "'s material");
        return nullptr;
    }
    return mat;
}

void SpriteRenderer::SetMaterial(std::string& id){
    Material* mat = SharedResources::Get().GetMaterial(id);
    if (!mat){
        Console::Alert("Unable to load " + GetGameObject()->GetName() + "'s Material");
        return;
    }
    material_id = mat->GetID();
}


Sprite* SpriteRenderer::GetSprite(){
    Sprite* sprite = SharedResources::Get().GetSprite(sprite_id);
    if (!sprite){
        Console::Alert("Unable to load " + GetGameObject()->GetName() + "'s Sprite");
        return nullptr;
    }
    return sprite;
}

void SpriteRenderer::SetSprite(std::string& id){
    Sprite* sprite = SharedResources::Get().GetSprite(id);
    if (!sprite){
        Console::Alert("Unable to load " + GetGameObject()->GetName() + "'s Sprite");
        return;
    }
    sprite_id = sprite->GetID();
}

void SpriteRenderer::SetVisible(bool& val){
    visible = val;
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

    // --------------------------------------------------
    // Bind texture (sprite overrides material)
    // --------------------------------------------------
    tex->Bind(0);
    shader->SetTexture("uTexture", 0);

    // --------------------------------------------------
    // UVs (atlas + flipping + per-instance tweaks)
    // --------------------------------------------------
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

    uvScale *= this->uvScale;     // per-instance tweak
    uvOffset += this->uvOffset;   // per-instance tweak

    shader->SetVec2("uUVScale", uvScale);
    shader->SetVec2("uUVOffset", uvOffset);

    // --------------------------------------------------
    // PIXELS → WORLD SIZE (THIS IS THE IMPORTANT PART)
    // --------------------------------------------------
    Transform* transform = GetGameObject()->GetComponent<Transform>();
    if (!transform)
        return;

    // Sprite size in pixels (atlas-aware)
    const glm::vec2 pixelSize = sprite->GetPixelSize();

    // Convert pixels → world units
    glm::vec2 worldSize = PixelsToWorld(pixelSize);

    Console::Comment(std::to_string(worldSize.x) + " " + std::to_string(worldSize.y));

    shader->SetVec2("uSize", worldSize);
    shader->SetVec2("uPivot", sprite->GetPivot());

    // --------------------------------------------------
    // Per-instance color
    // --------------------------------------------------
    shader->SetVec4("uColor", color);
}


