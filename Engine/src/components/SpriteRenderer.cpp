#include "engine/components/SpriteRenderer.hpp"
#include "engine/rendering/core/SharedResources.hpp"
#include "engine/debug/Console.hpp"

YAML::Node SpriteRenderer::Serialize()
{
    YAML::Node node;
    return node;
}

void SpriteRenderer::Deserialize(const YAML::Node& node)
{
    Component::Deserialize(node);


    material_id = node["material_id"].as<std::string>();
    texture_id = node["texture_id"].as<std::string>();
    color = glm::vec4( node["color"][0].as<float>(), node["color"][1].as<float>(), node["color"][2].as<float>(), node["color"][3].as<float>() );
    flipX = node["flipX"].as<bool>();
    flipY = node["flipY"].as<bool>();
}

void SpriteRenderer::PostDeserialize()
{
    material = SharedResources::Get().GetMaterial(material_id);
    texture = SharedResources::Get().GetTexture(texture_id);
    if(!material) Console::Alert("No Material Loaded");
    if(!texture) Console::Alert("No Texture Loaded"); 
}

void SpriteRenderer::SetTexture(Texture2D* tex){
    if (!tex){
        Console::Alert("Unable to set Texture");
    }

    texture = tex;
    texture_id = tex->GetID();

}


void SpriteRenderer::SetMaterial(Material* mat){
    if (!mat){
        Console::Alert("Unable to set Material");
    }

    material = mat;
    material_id = mat->GetID();

}


