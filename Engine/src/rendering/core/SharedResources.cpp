#include "engine/rendering/core/SharedResources.hpp"
#include "engine/debug/Console.hpp"

#define RESOURCES_CONFIG_PATH "Domain/resources_config.yaml"

void SharedResources::Init()
{
    const YAML::Node root = YAML::LoadFile(RESOURCES_CONFIG_PATH);
    const YAML::Node data = root["Resources"];

    for (auto& texNode : data["Textures"])
    {
        Texture2D* texture = new Texture2D();
        texture->Deserialize(texNode);
        AddTexture(texture);
        std::cout << texture->GetName() << std::endl;
    }

    for (auto& shadNode : data["Shaders"])
    {
        Shader* shader = new Shader();
        shader->Deserialize(shadNode);
        AddShader(shader);
    }

    for (auto& matNode : data["Materials"])
    {
        Material* material = new Material();
        material->Deserialize(matNode);
        AddMaterial(material);
    }

    std::vector<Serializable*> all;

    for (auto& kv : textures)  all.push_back(kv.second);
    for (auto& kv : shaders)   all.push_back(kv.second);
    for (auto& kv : materials) all.push_back(kv.second);

    for (Serializable* obj : all)
        obj->PostDeserialize();

    for (Serializable* obj : all)
        Console::Comment("HEYYY");
    Console::Comment("Shared Resources Initialized");
    
}

// ----------------------
// Lookup By ID
// ----------------------

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
