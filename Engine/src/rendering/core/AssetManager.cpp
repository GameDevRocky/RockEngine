#include "engine/rendering/core/AssetManager.hpp"
#include "engine/debug/Console.hpp"
#include "engine/rendering/core/Resource.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "engine/rendering/core/AssetMetaService.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;
using namespace EngineUtils;

void AssetManager::Deserialize(const YAML::Node& node){
    const YAML::Node root = node;
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
void AssetManager::Init()
{    
    
    std::vector<Resource*> all;
    
    for (auto& kv : textures)  all.push_back(kv.second);
    for (auto& kv : sprites)  all.push_back(kv.second);
    for (auto& kv : shaders)   all.push_back(kv.second);
    for (auto& kv : materials) all.push_back(kv.second);
    
    for (Resource* obj : all) obj->Init();
    
    std::cout << "Shared Resources Initialized" << std::endl;
    
}
void AssetManager::Awake(){

    std::vector<Resource*> all;
    
    for (auto& kv : textures)  all.push_back(kv.second);
    for (auto& kv : sprites)  all.push_back(kv.second);
    for (auto& kv : shaders)   all.push_back(kv.second);
    for (auto& kv : materials) all.push_back(kv.second);
    
    for (Resource* obj : all) obj->Awake();
}


Shader* AssetManager::GetShader(const std::string& id)
{
    auto it = shaders.find(id);
    return (it != shaders.end()) ? it->second : nullptr;
}

Texture2D* AssetManager::GetTexture(const std::string& id)
{
    auto it = textures.find(id);
    return (it != textures.end()) ? it->second : nullptr;
}

Material* AssetManager::GetMaterial(const std::string& id)
{
    auto it = materials.find(id);
    return (it != materials.end()) ? it->second : nullptr;
}

Sprite* AssetManager::GetSprite(const std::string& id)
{
    auto it = sprites.find(id);
    return (it != sprites.end()) ? it->second : nullptr;
}

// ----------------------
// Lookup By Name
// ----------------------

Shader* AssetManager::GetShaderByName(const std::string& name)
{
    for (auto& kv : shaders)
        if (kv.second->GetName() == name)
            return kv.second;
    return nullptr;
}

Texture2D* AssetManager::GetTextureByName(const std::string& name)
{
    for (auto& kv : textures)
        if (kv.second->GetName() == name)
            return kv.second;
    return nullptr;
}

Material* AssetManager::GetMaterialByName(const std::string& name)
{
    for (auto& kv : materials)
        if (kv.second->GetName() == name)
            return kv.second;
    return nullptr;
}
Sprite* AssetManager::GetSpriteByName(const std::string& name)
{
    for (auto& kv : sprites)
        if (kv.second->GetName() == name)
            return kv.second;
    return nullptr;
}

// ----------------------
// Add
// ----------------------

void AssetManager::AddShader(Shader* shader)
{
    if (!shader)
    {
        Console::Alert("Failed to add shader");
        return;
    }
    shaders[shader->GetID()] = shader;
    Notify(ASSET_ADDED_EVENT, static_cast<Resource*>(shader));
}
void AssetManager::AddSprite(Sprite* sprite)
{
    if (!sprite)
    {
        Console::Alert("Failed to add sprite");
        return;
    }
    sprites[sprite->GetID()] = sprite;
    Notify(ASSET_ADDED_EVENT, static_cast<Resource*>(sprite));
}

void AssetManager::AddTexture(Texture2D* texture)
{
    if (!texture)
    {
        Console::Alert("Failed to add texture");
        return;
    }
    textures[texture->GetID()] = texture;
    Notify(ASSET_ADDED_EVENT, static_cast<Resource*>(texture));
}

void AssetManager::AddMaterial(Material* material)
{
    if (!material)
    {
        Console::Alert("Failed to add material");
        return;
    }
    materials[material->GetID()] = material;
    Notify(ASSET_ADDED_EVENT, static_cast<Resource*>(material));
}


void AssetManager::LoadAsset(const YAML::Node& node, const std::string& type) {
    if (type == "material") {
        LoadMaterial(node);
    } else if (type == "texture2d") {
        LoadTexture(node);
    } else if (type == "shader") {
        LoadShader(node);
    }
}

void AssetManager::LoadMaterial(const YAML::Node& node, const std::string& filePath) {
    if (node["id"] && materials.count(node["id"].as<std::string>())) return;

    Material* mat = new Material();
    mat->Deserialize(node);
    if (!filePath.empty()) mat->SetFilePath(filePath);
    mat->Init();
    mat->Awake();
    AddMaterial(mat);
    std::cout << "Loaded and Registered Material: " + mat->GetName() << std::endl;
}

void AssetManager::LoadTexture(const YAML::Node& node, const std::string& filePath) {
    if (node["id"] && textures.count(node["id"].as<std::string>())) return;

    Texture2D* tex = new Texture2D();
    tex->Deserialize(node);
    if (!filePath.empty()) tex->SetFilePath(filePath);
    tex->Init();
    tex->Awake();
    AddTexture(tex);
    std::cout << "Loaded and Registered Texture: " + tex->GetName() << std::endl;

    if (node["sprites"]) {
        for (const auto& spriteNode : node["sprites"]) {
            LoadSprite(spriteNode, filePath);
            if (spriteNode["id"]) {
                Sprite* s = GetSprite(spriteNode["id"].as<std::string>());
                if (s) {
                    std::string texId = tex->GetID();
                    s->SetTexture(texId);
                }
            }
        }
    }
}

void AssetManager::LoadShader(const YAML::Node& node, const std::string& filePath) {
    if (node["id"] && shaders.count(node["id"].as<std::string>())) return;

    Shader* shader = new Shader();
    shader->Deserialize(node);
    if (!filePath.empty()) shader->SetFilePath(filePath);
    shader->Init();
    AddShader(shader);
    std::cout << "Loaded and Registered Shader: " + shader->GetName() << std::endl;
}

void AssetManager::LoadSprite(const YAML::Node& node, const std::string& filePath) {
    if (node["id"] && sprites.count(node["id"].as<std::string>())) return;

    Sprite* sprite = new Sprite();
    sprite->Deserialize(node);
    if (!filePath.empty()) sprite->SetFilePath(filePath);
    sprite->Init();
    sprite->Awake();
    AddSprite(sprite);
    std::cout << "Loaded and Registered Sprite: " + sprite->GetName() << std::endl;
}

void AssetManager::LoadAssetFromFile(const std::string& filePath) {
    YAML::Node node;
    try {
        node = YAML::LoadFile(filePath);
    } catch (const std::exception& e) {
        Console::Alert("Failed to parse asset file: " + filePath + " – " + e.what());
        return;
    }
    if (!node["type"]) {
        Console::Alert("Asset file missing 'type' field: " + filePath);
        return;
    }
    std::string type = node["type"].as<std::string>();
    std::transform(type.begin(), type.end(), type.begin(), ::tolower);

    if (type == "material") {
        LoadMaterial(node, filePath);
    } else if (type == "texture2d") {
        LoadTexture(node, filePath);
    } else if (type == "shader") {
        LoadShader(node, filePath);
    }
}

void AssetManager::LoadFromDirectory(const std::string& rootDir) {
    fs::path root(rootDir);
    if (!fs::exists(root) || !fs::is_directory(root)) {
        Console::Alert("LoadFromDirectory: invalid directory: " + rootDir);
        return;
    }

    // Load in dependency order: textures (+ their embedded sprites) and shaders
    // first, then materials.
    std::vector<fs::path> textureMetas, shaderMetas, materialMetas;

    std::error_code ec;
    for (auto& entry : fs::recursive_directory_iterator(root, ec)) {
        if (!entry.is_regular_file()) continue;
        const std::string ext = entry.path().extension().string();
        if      (ext == ".texture")  textureMetas .push_back(entry.path());
        else if (ext == ".shader")   shaderMetas  .push_back(entry.path());
        else if (ext == ".material" || ext == ".mat") materialMetas.push_back(entry.path());
    }
    if (ec)
        Console::Alert("LoadFromDirectory: iteration error: " + ec.message());

    for (auto& p : textureMetas)  LoadAssetFromFile(p.string());
    for (auto& p : shaderMetas)   LoadAssetFromFile(p.string());
    for (auto& p : materialMetas) LoadAssetFromFile(p.string());
}