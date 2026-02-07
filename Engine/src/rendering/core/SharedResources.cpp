#include "engine/rendering/core/SharedResources.hpp"
#include "engine/debug/Console.hpp"
#include "engine/rendering/core/Resource.hpp"
#define RESOURCES_CONFIG_PATH "Domain/lib/configs/resources_config.yaml"

void SharedResources::Deserialize(const YAML::Node& node){
    const YAML::Node root = YAML::LoadFile(RESOURCES_CONFIG_PATH);
    const YAML::Node data = root["Resources"];
    
    for (auto& texNode : data["Textures"])
    {
        Texture2D* texture = new Texture2D();
        texture->Deserialize(texNode);
        AddTexture(texture);
        std::cout << "Loaded and Registered Texture: " + texture->GetName() << std::endl;
    }
    for (auto& spriteNode : data["Sprites"])
    {
        Sprite* sprite = new Sprite();
        sprite->Deserialize(spriteNode);
        AddSprite(sprite);
        std::cout << "Loaded and Registered Sprite: " + sprite->GetName() << std::endl;
    }
    
    for (auto& shadNode : data["Shaders"])
    {
        Shader* shader = new Shader();
        shader->Deserialize(shadNode);
        AddShader(shader);
        std::cout << "Loaded and Registered Shader: " + shader->GetName() << std::endl;
        
    }
    
    for (auto& matNode : data["Materials"])
    {
        Material* material = new Material();
        material->Deserialize(matNode);
        AddMaterial(material);
        std::cout << "Loaded and Registered Material: " + material->GetName() << std::endl;
    }
}
void SharedResources::Init()
{    
    
    std::vector<Resource*> all;
    
    for (auto& kv : textures)  all.push_back(kv.second);
    for (auto& kv : sprites)  all.push_back(kv.second);
    for (auto& kv : shaders)   all.push_back(kv.second);
    for (auto& kv : materials) all.push_back(kv.second);
    
    for (Resource* obj : all) obj->Init();
    
    std::cout << "Shared Resources Initialized" << std::endl;
    
}
void SharedResources::Awake(){

    std::vector<Resource*> all;
    
    for (auto& kv : textures)  all.push_back(kv.second);
    for (auto& kv : sprites)  all.push_back(kv.second);
    for (auto& kv : shaders)   all.push_back(kv.second);
    for (auto& kv : materials) all.push_back(kv.second);
    
    for (Resource* obj : all) obj->Awake();
}


Shader* SharedResources::GetShader(const std::string& id)
{
    auto it = shaders.find(id);
    return (it != shaders.end()) ? it->second : nullptr;
}

Texture2D* SharedResources::GetTexture(const std::string& id)
{
    auto it = textures.find(id);
    return (it != textures.end()) ? it->second : nullptr;
}

Material* SharedResources::GetMaterial(const std::string& id)
{
    auto it = materials.find(id);
    return (it != materials.end()) ? it->second : nullptr;
}

Sprite* SharedResources::GetSprite(const std::string& id)
{
    auto it = sprites.find(id);
    return (it != sprites.end()) ? it->second : nullptr;
}

// ----------------------
// Lookup By Name
// ----------------------

Shader* SharedResources::GetShaderByName(const std::string& name)
{
    for (auto& kv : shaders)
        if (kv.second->GetName() == name)
            return kv.second;
    return nullptr;
}

Texture2D* SharedResources::GetTextureByName(const std::string& name)
{
    for (auto& kv : textures)
        if (kv.second->GetName() == name)
            return kv.second;
    return nullptr;
}

Material* SharedResources::GetMaterialByName(const std::string& name)
{
    for (auto& kv : materials)
        if (kv.second->GetName() == name)
            return kv.second;
    return nullptr;
}
Sprite* SharedResources::GetSpriteByName(const std::string& name)
{
    for (auto& kv : sprites)
        if (kv.second->GetName() == name)
            return kv.second;
    return nullptr;
}

// ----------------------
// Add
// ----------------------

void SharedResources::AddShader(Shader* shader)
{
    if (!shader)
    {
        Console::Alert("Failed to add shader");
        return;
    }
    shaders[shader->GetID()] = shader;
}
void SharedResources::AddSprite(Sprite* sprite)
{
    if (!sprite)
    {
        Console::Alert("Failed to add sprite");
        return;
    }
    sprites[sprite->GetID()] = sprite;
}

void SharedResources::AddTexture(Texture2D* texture)
{
    if (!texture)
    {
        Console::Alert("Failed to add texture");
        return;
    }
    textures[texture->GetID()] = texture;
}

void SharedResources::AddMaterial(Material* material)
{
    if (!material)
    {
        Console::Alert("Failed to add material");
        return;
    }
    materials[material->GetID()] = material;
}
