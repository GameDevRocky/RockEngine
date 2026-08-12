#pragma once
#include <string>
#include <unordered_map>
#include <yaml-cpp/yaml.h>
#include <filesystem>

#include "engine/core/System.hpp"
#include "engine/rendering/core/Shader.hpp"
#include "engine/rendering/core/Texture2D.hpp"
#include "engine/rendering/core/Material.hpp"
#include "engine/rendering/core/Sprite.hpp"
#include "engine/rendering/core/Font.hpp"

class AssetManager : public System
{
public:
    static inline const Event ASSET_ADDED_EVENT   = Observable::CreateEvent();
    static inline const Event ASSET_REMOVED_EVENT = Observable::CreateEvent();

    static AssetManager& Get()
    {
        static AssetManager instance;
        return instance;
    }
    void Deserialize(const YAML::Node& node) override;
    void Init() override;
    void Awake() override;
    
    Shader* GetShader(const std::string& id);
    Texture2D* GetTexture(const std::string& id);
    Material* GetMaterial(const std::string& id);
    Sprite* GetSprite(const std::string& id);
    Font* GetFont(const std::string& id);

    Shader* GetShaderByName(const std::string& name);
    Texture2D* GetTextureByName(const std::string& name);
    Material* GetMaterialByName(const std::string& name);
    Sprite* GetSpriteByName(const std::string& name);
    Font* GetFontByName(const std::string& name);

    const std::unordered_map<std::string, Material*>& GetAllMaterials() const { return materials; }
    const std::unordered_map<std::string, Sprite*>&   GetAllSprites()   const { return sprites; }
    const std::unordered_map<std::string, Texture2D*>& GetAllTextures() const { return textures; }
    const std::unordered_map<std::string, Shader*>&   GetAllShaders()   const { return shaders; }
    const std::unordered_map<std::string, Font*>&     GetAllFonts()     const { return fonts; }

    void AddShader(Shader* shader);
    void AddTexture(Texture2D* texture);
    void AddMaterial(Material* material);
    void AddSprite(Sprite* sprite);
    void AddFont(Font* font);

    // Create a new sprite carving a normalized-UV region out of an existing texture,
    // register it with that texture, and persist it into the texture's meta file.
    // Returns nullptr if the texture id is unknown.
    Sprite* CreateSprite(const std::string& textureId, glm::vec2 uvMin, glm::vec2 uvMax,
                         const std::string& name = "");

    // Delete a sprite: drop it from its owning texture, rewrite the texture file, fire
    // ASSET_REMOVED_EVENT (payload = id), and free it. SpriteRenderers that still
    // reference the id simply resolve to nullptr afterwards.
    void RemoveSprite(const std::string& id);

    // Remove an asset from memory by id, whichever kind it is (material, texture,
    // shader, or sprite). Removing a texture also removes every sprite carved from
    // it. Fires ASSET_REMOVED_EVENT (payload = id) and frees the object; anything
    // still referencing the id resolves to nullptr. Does NOT touch disk -- the
    // caller deletes the file(s). No-op for an unknown id.
    void RemoveAsset(const std::string& id);

    // Create a brand-new material as a .material file at filePath, register it,
    // and return it. Defaults to the "sprite" shader when present so it renders
    // (and its uniform defaults get seeded) out of the box. For the editor's
    // "New > Material".
    Material* CreateMaterial(const std::string& filePath, const std::string& name);

    // Import an external image into destDir: copy the file in (unless it already
    // lives there), generate its .texture meta, register the texture, and return
    // it (nullptr on failure). Powers dropping images onto the Folder view.
    Texture2D* ImportTexture(const std::string& sourceFile, const std::string& destDir);

    void LoadAsset(const YAML::Node& node, const std::string& type);
    void LoadAssetFromFile(const std::string& filePath);

    // Serialize a resource back to its source meta file. A Sprite has no file of its
    // own, so it re-saves the parent texture (which embeds all its sprites).
    void SaveResource(Resource* r);
    // Arm auto-save; called once after the initial LoadFromDirectory so bootstrap
    // notifications don't rewrite every asset file on startup.
    void EnableAutoSave() { m_autoSaveEnabled = true; }

    // Scan a directory recursively for meta files (.texture, .shader, .material,
    // .mat, .sprite) and load any assets not already registered.
    void LoadFromDirectory(const std::string& rootDir);

    private:
    // The two halves of LoadTexture either side of the decode+upload, so the
    // bulk load in LoadFromDirectory can construct every texture, decode them
    // all in parallel, then upload serially on the thread that owns the GL
    // context. See ImageDecoder for why the decode is separable at all.
    Texture2D* CreateTextureFromMeta(const YAML::Node& node, const std::string& filePath);
    void FinishTextureLoad(Texture2D* tex, const YAML::Node& node, const std::string& filePath);

    void LoadMaterial(const YAML::Node& node, const std::string& filePath = {});
    void LoadTexture (const YAML::Node& node, const std::string& filePath = {});
    void LoadShader  (const YAML::Node& node, const std::string& filePath = {});
    void LoadSprite  (const YAML::Node& node, const std::string& filePath = {});
    void LoadFont    (const YAML::Node& node, const std::string& filePath = {});

    // Persist the resource whenever it fires a change notification (once auto-save is armed).
    void SubscribeAutoSave(Resource* r);
    bool m_autoSaveEnabled = false;

    AssetManager() = default;

    std::unordered_map<std::string, Shader*> shaders;
    std::unordered_map<std::string, Texture2D*> textures;
    std::unordered_map<std::string, Material*> materials;
    std::unordered_map<std::string, Sprite*> sprites;
    std::unordered_map<std::string, Font*> fonts;
};
