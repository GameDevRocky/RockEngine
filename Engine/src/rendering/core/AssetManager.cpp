#include "engine/rendering/core/AssetManager.hpp"
#include "engine/debug/Console.hpp"
#include "engine/rendering/core/Resource.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "engine/rendering/core/AssetMetaService.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
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
    SubscribeAutoSave(shader);
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
    SubscribeAutoSave(sprite);
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
    SubscribeAutoSave(texture);
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
    SubscribeAutoSave(material);
    Notify(ASSET_ADDED_EVENT, static_cast<Resource*>(material));
}

Sprite* AssetManager::CreateSprite(const std::string& textureId, glm::vec2 uvMin,
                                   glm::vec2 uvMax, const std::string& name)
{
    Texture2D* tex = GetTexture(textureId);
    if (!tex) {
        Console::Alert("CreateSprite: unknown texture id " + textureId);
        return nullptr;
    }

    Sprite* sprite = new Sprite();
    std::string id = GenerateUUID();
    sprite->SetID(id);
    sprite->SetName(name.empty() ? id : name);
    sprite->SetUVMin(uvMin);
    sprite->SetUVMax(uvMax);
    sprite->SetPivot({0.0f, 0.0f});

    std::string texId = tex->GetID();
    sprite->SetTexture(texId);                 // populates texture_id
    sprite->SetFilePath(tex->GetFilePath());

    tex->RegisterSprite(id);
    AddSprite(sprite);                          // subscribes auto-save + fires ASSET_ADDED_EVENT
    SaveResource(tex);                          // persist the new sprite immediately
    return sprite;
}

void AssetManager::RemoveSprite(const std::string& id)
{
    auto it = sprites.find(id);
    if (it == sprites.end()) return;

    Sprite* sprite = it->second;
    Texture2D* tex = GetTexture(sprite->GetTextureID());

    // Drop the sprite from the registry and its owning texture, then rewrite the
    // texture file so its embedded sprites: sequence no longer contains it.
    if (tex) tex->UnregisterSprite(id);
    sprites.erase(it);
    if (tex) SaveResource(tex);

    Notify(ASSET_REMOVED_EVENT, id);
    delete sprite;
}

Material* AssetManager::CreateMaterial(const std::string& filePath, const std::string& name) {
    Material* mat = new Material();

    YAML::Node node;
    node["type"] = "Material";
    node["name"] = name;
    node["id"]   = GenerateUUID();
    // Default to the sprite shader when it exists so the material is usable and
    // Awake()→SetShader()→Validate() can seed its uniform defaults; otherwise it
    // is created blank and the user assigns a shader in the inspector.
    if (Shader* s = GetShaderByName("sprite")) node["shader_id"] = s->GetID();

    mat->Deserialize(node);
    mat->SetFilePath(filePath);
    mat->Init();
    mat->Awake();          // resolves the shader and seeds uniform defaults
    AddMaterial(mat);      // registers + fires ASSET_ADDED_EVENT + arms auto-save
    SaveResource(mat);     // write the canonical, fully-seeded file to disk
    std::cout << "Created Material: " + filePath << std::endl;
    return mat;
}

Texture2D* AssetManager::ImportTexture(const std::string& sourceFile, const std::string& destDir) {
    std::error_code ec;
    fs::path src(sourceFile);
    if (!fs::is_regular_file(src, ec)) {
        Console::Alert("ImportTexture: source not found: " + sourceFile);
        return nullptr;
    }

    fs::path dest = fs::path(destDir) / src.filename();

    // Copy into the folder unless the source already lives there (dragged from
    // within the project). Never clobber an existing file of the same name.
    const bool sameFile = fs::weakly_canonical(src, ec) == fs::weakly_canonical(dest, ec);
    if (!sameFile) {
        if (fs::exists(dest, ec)) {
            Console::Alert("ImportTexture: '" + dest.filename().string() + "' already exists here.");
            return nullptr;
        }
        fs::copy_file(src, dest, ec);
        if (ec) {
            Console::Alert("ImportTexture: copy failed: " + ec.message());
            return nullptr;
        }
    }

    // Generate the .texture meta for just this file, then register the texture
    // (LoadAssetFromFile dedupes by id, so a re-import is a no-op).
    AssetMetaService::GenerateFor(dest.string());
    const fs::path metaPath(dest.string() + ".texture");
    if (!fs::exists(metaPath, ec)) {
        Console::Alert("ImportTexture: failed to generate meta for: " + dest.string());
        return nullptr;
    }
    LoadAssetFromFile(metaPath.string());

    try {
        YAML::Node meta = YAML::LoadFile(metaPath.string());
        if (meta["id"]) return GetTexture(meta["id"].as<std::string>());
    } catch (...) {}
    return nullptr;
}

void AssetManager::RemoveAsset(const std::string& id) {
    if (auto it = materials.find(id); it != materials.end()) {
        Material* m = it->second;
        materials.erase(it);
        Notify(ASSET_REMOVED_EVENT, id);
        delete m;
        return;
    }

    if (auto it = textures.find(id); it != textures.end()) {
        Texture2D* tex = it->second;

        // Cascade: every sprite carved from this texture goes with it. Collect
        // first, then erase (don't mutate the map mid-iteration). Erase directly
        // rather than via RemoveSprite -- that would re-save the texture file
        // we're about to delete.
        std::vector<std::string> owned;
        for (auto& [sid, sp] : sprites)
            if (sp && sp->GetTextureID() == id) owned.push_back(sid);
        for (const std::string& sid : owned) {
            Sprite* sp = sprites[sid];
            sprites.erase(sid);
            Notify(ASSET_REMOVED_EVENT, sid);
            delete sp;
        }

        textures.erase(id);
        Notify(ASSET_REMOVED_EVENT, id);
        delete tex;
        return;
    }

    if (auto it = shaders.find(id); it != shaders.end()) {
        Shader* s = it->second;
        shaders.erase(it);
        Notify(ASSET_REMOVED_EVENT, id);
        delete s;
        return;
    }

    // A standalone sprite -- reuse the sprite path (it rewrites the parent texture).
    if (sprites.count(id)) RemoveSprite(id);
}

void AssetManager::SubscribeAutoSave(Resource* r) {
    if (!r) return;
    r->Subscribe([this, r]() {
        if (m_autoSaveEnabled) SaveResource(r);
        return true;
    });
}

void AssetManager::SaveResource(Resource* r) {
    if (!r) return;

    // Sprites are embedded in their texture's file — persist the texture instead.
    if (Sprite* sprite = dynamic_cast<Sprite*>(r)) {
        if (Texture2D* tex = GetTexture(sprite->GetTextureID()))
            SaveResource(tex);
        return;
    }

    const std::string& path = r->GetFilePath();
    if (path.empty()) return;

    std::ofstream fout(path);
    if (!fout.is_open()) {
        Console::Alert("Failed to save asset to disk: " + path);
        return;
    }
    fout << r->Serialize();
    std::cout << "Saved asset to: " + path << std::endl;
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
                    tex->RegisterSprite(s->GetID());
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

    // Everything is loaded; from here on, in-memory edits sync back to disk.
    EnableAutoSave();
}