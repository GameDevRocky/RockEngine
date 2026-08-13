#include "engine/rendering/core/AssetManager.hpp"
#include "engine/debug/Console.hpp"
#include "engine/rendering/core/Resource.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "engine/rendering/core/AssetMetaService.hpp"
#include "engine/rendering/core/ImageDecoder.hpp"
#include "engine/jobs/BootProgress.hpp"
#include "engine/jobs/ParallelFor.hpp"
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
    for (auto& kv : fonts)     all.push_back(kv.second);
    for (auto& kv : audioClips) all.push_back(kv.second);

    for (Resource* obj : all) obj->Init();

    std::cout << "Shared Resources Initialized" << std::endl;

}
void AssetManager::Awake(){

    std::vector<Resource*> all;

    for (auto& kv : textures)  all.push_back(kv.second);
    for (auto& kv : sprites)  all.push_back(kv.second);
    for (auto& kv : shaders)   all.push_back(kv.second);
    for (auto& kv : materials) all.push_back(kv.second);
    for (auto& kv : fonts)     all.push_back(kv.second);
    for (auto& kv : audioClips) all.push_back(kv.second);

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

Font* AssetManager::GetFont(const std::string& id)
{
    auto it = fonts.find(id);
    return (it != fonts.end()) ? it->second : nullptr;
}

Font* AssetManager::GetFontByName(const std::string& name)
{
    for (auto& kv : fonts)
        if (kv.second->GetName() == name)
            return kv.second;
    return nullptr;
}

AudioClip* AssetManager::GetAudioClip(const std::string& id)
{
    auto it = audioClips.find(id);
    return (it != audioClips.end()) ? it->second : nullptr;
}

AudioClip* AssetManager::GetAudioClipByName(const std::string& name)
{
    for (auto& kv : audioClips)
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

void AssetManager::AddFont(Font* font)
{
    if (!font)
    {
        Console::Alert("Failed to add font");
        return;
    }
    fonts[font->GetID()] = font;
    SubscribeAutoSave(font);
    Notify(ASSET_ADDED_EVENT, static_cast<Resource*>(font));
}

void AssetManager::AddAudioClip(AudioClip* clip)
{
    if (!clip)
    {
        Console::Alert("Failed to add audio clip");
        return;
    }
    audioClips[clip->GetID()] = clip;
    SubscribeAutoSave(clip);
    Notify(ASSET_ADDED_EVENT, static_cast<Resource*>(clip));
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

    if (auto it = fonts.find(id); it != fonts.end()) {
        Font* f = it->second;
        fonts.erase(it);
        Notify(ASSET_REMOVED_EVENT, id);
        delete f;
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
    } else if (type == "font") {
        LoadFont(node);
    } else if (type == "audioclip") {
        LoadAudioClip(node);
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
    Texture2D* tex = CreateTextureFromMeta(node, filePath);
    if (!tex) return;
    tex->Awake();                       // decode + upload, synchronously
    FinishTextureLoad(tex, node, filePath);
}

// Everything about loading a texture EXCEPT the decode and the GL upload. Split
// out so the bulk load can construct all of them first, decode them in parallel,
// and upload afterwards -- see LoadFromDirectory. Returns nullptr if the id is
// already registered.
Texture2D* AssetManager::CreateTextureFromMeta(const YAML::Node& node,
                                               const std::string& filePath) {
    if (node["id"] && textures.count(node["id"].as<std::string>())) return nullptr;

    Texture2D* tex = new Texture2D();
    tex->Deserialize(node);             // pure CPU: resolves the source path
    if (!filePath.empty()) tex->SetFilePath(filePath);
    tex->Init();                        // no-op; Texture2D doesn't override it
    return tex;
}

void AssetManager::FinishTextureLoad(Texture2D* tex, const YAML::Node& node,
                                     const std::string& filePath) {
    if (!tex) return;
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

void AssetManager::LoadFont(const YAML::Node& node, const std::string& filePath) {
    if (node["id"] && fonts.count(node["id"].as<std::string>())) return;

    Font* font = new Font();
    font->Deserialize(node);
    if (!filePath.empty()) font->SetFilePath(filePath);
    font->Init();
    // No Awake() call: unlike a texture, a font does no GL work at load. The
    // atlas is baked and uploaded lazily by EnsureUploaded() on first use, so a
    // font nobody references never costs anything.
    AddFont(font);
    std::cout << "Loaded and Registered Font: " + font->GetName() << std::endl;
}

void AssetManager::LoadAudioClip(const YAML::Node& node, const std::string& filePath) {
    if (node["id"] && audioClips.count(node["id"].as<std::string>())) return;

    AudioClip* clip = new AudioClip();
    clip->Deserialize(node);
    if (!filePath.empty()) clip->SetFilePath(filePath);
    clip->Init();
    clip->Awake();          // probes duration/channels/sample rate -- cheap, header-only
    AddAudioClip(clip);
    std::cout << "Loaded and Registered AudioClip: " + clip->GetName() << std::endl;
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
    } else if (type == "font") {
        LoadFont(node, filePath);
    } else if (type == "audioclip") {
        LoadAudioClip(node, filePath);
    }
}

void AssetManager::LoadFromDirectory(const std::string& rootDir) {
    fs::path root(rootDir);
    if (!fs::exists(root) || !fs::is_directory(root)) {
        Console::Alert("LoadFromDirectory: invalid directory: " + rootDir);
        return;
    }

    BootProgress::Report(-1.0f, "Scanning assets");

    // Load in dependency order: textures (+ their embedded sprites) and shaders
    // first, then materials. Fonts and audio clips depend on nothing and nothing
    // depends on them at load time (a TextRenderer/AudioSource resolves its
    // font/clip by id when used), so they can go anywhere in the order.
    std::vector<fs::path> textureMetas, shaderMetas, materialMetas, fontMetas, audioMetas;

    std::error_code ec;
    for (auto& entry : fs::recursive_directory_iterator(root, ec)) {
        if (!entry.is_regular_file()) continue;
        const std::string ext = entry.path().extension().string();
        if      (ext == ".texture")  textureMetas .push_back(entry.path());
        else if (ext == ".shader")   shaderMetas  .push_back(entry.path());
        else if (ext == ".font")     fontMetas    .push_back(entry.path());
        else if (ext == ".audio")    audioMetas   .push_back(entry.path());
        else if (ext == ".material" || ext == ".mat") materialMetas.push_back(entry.path());
    }
    if (ec)
        Console::Alert("LoadFromDirectory: iteration error: " + ec.message());

    // Reported so the startup splash can show real progress. This runs before
    // app->exec(), so there is no event loop and no job pump -- BootProgress is
    // a direct call and the splash force-repaints itself on each one.
    const std::size_t total = textureMetas.size() + shaderMetas.size()
                            + fontMetas.size() + materialMetas.size() + audioMetas.size();
    std::size_t done = 0;
    auto loadAll = [&](const std::vector<fs::path>& metas, const char* kind) {
        for (const auto& p : metas) {
            BootProgress::Report(total ? static_cast<float>(done) / static_cast<float>(total) : -1.0f,
                                 std::string(kind) + ": " + p.stem().string());
            LoadAssetFromFile(p.string());
            ++done;
        }
    };

    // ── Textures: decode in parallel, upload serially ───────────────────────
    // The one genuinely parallel step in the whole boot. Decoding ~210 PNGs
    // (25MB) is pure CPU and was the single largest stall in startup; the GL
    // upload that follows is not parallelizable and must stay on the thread that
    // owns the context, so the two are separated rather than interleaved.
    {
        struct Pending { Texture2D* tex; YAML::Node node; std::string metaPath; };
        std::vector<Pending> pending;
        pending.reserve(textureMetas.size());

        // Phase A, main thread: parse the metas and construct the objects. Both
        // steps allocate Serializables, which call GenerateUUID -- so neither
        // can move off this thread.
        for (const auto& p : textureMetas) {
            BootProgress::Report(static_cast<float>(done) / static_cast<float>(total ? total : 1),
                                 "Reading " + p.stem().string());
            try {
                YAML::Node node = YAML::LoadFile(p.string());
                if (Texture2D* tex = CreateTextureFromMeta(node, p.string()))
                    pending.push_back({tex, node, p.string()});
            } catch (const std::exception& e) {
                Console::Alert("Failed to read texture meta " + p.string() + ": " + e.what());
            }
            ++done;
        }

        BootProgress::Report(static_cast<float>(done) / static_cast<float>(total ? total : 1),
                             "Decoding " + std::to_string(pending.size()) + " images");

        // Phase B, worker threads: pure decode. Each index writes only its own
        // slot, and nothing here touches engine state -- which is exactly the
        // contract ParallelFor and ImageDecoder are both documented against.
        std::vector<DecodedImage> images(pending.size());
        ParallelFor::Run(pending.size(), [&](std::size_t i) {
            images[i] = ImageDecoder::Decode(pending[i].tex->GetPath());
        });

        // Phase C, main thread: upload and register. AddTexture fires
        // ASSET_ADDED_EVENT, so this half could never have been threaded anyway.
        for (std::size_t i = 0; i < pending.size(); ++i) {
            if (!images[i].ok) {
                Console::Alert("Texture decode failed: " + images[i].error);
                delete pending[i].tex;
                continue;
            }
            pending[i].tex->UploadDecoded(images[i]);
            FinishTextureLoad(pending[i].tex, pending[i].node, pending[i].metaPath);
        }
    }

    loadAll(shaderMetas,   "Shaders");
    loadAll(fontMetas,     "Fonts");
    loadAll(audioMetas,    "Audio");
    loadAll(materialMetas, "Materials");

    BootProgress::Report(1.0f, "Finishing up");

    // Everything is loaded; from here on, in-memory edits sync back to disk.
    EnableAutoSave();
}