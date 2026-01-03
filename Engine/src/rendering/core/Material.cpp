#include "engine/rendering/core/Material.hpp"
#include "engine/rendering/core/SharedResources.hpp"
#include "engine/debug/Console.hpp"


void Material::Deserialize(const YAML::Node& node){
    Serializable::Deserialize(node);
    name = node["name"].as<std::string>();
    shader_id = node["shader_id"].as<std::string>();
    texture_id = node["texture_id"].as<std::string>();
}

void Material::PostDeserialize(){
    shader = SharedResources::Get().GetShader(shader_id);
    texture = SharedResources::Get().GetTexture(texture_id);
    SetShader(shader);
    SetTexture(texture);
    if (!shader) Console::Alert("SHADER NOT FOUND " );

}

void Material::Init(){
    


}

void Material::SetShader(Shader* s){
    if (!s) return;
    shader = s;
    shader_id = s->GetID();
}

void Material::SetTexture(Texture2D* t){
    if (!t) return;
    texture = t;
    texture_id = t->GetID();
}

