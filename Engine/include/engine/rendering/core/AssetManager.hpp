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

class AssetManager : public System
{
public:
    static inline const Event ASSET_ADDED_EVENT = Observable::CreateEvent();

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

    Shader* GetShaderByName(const std::string& name);
    Texture2D* GetTextureByName(const std::string& name);
    Material* GetMaterialByName(const std::string& name);
    Sprite* GetSpriteByName(const std::string& name);

    const std::unordered_map<std::string, Material*>& GetAllMaterials() const { return materials; }
    const std::unordered_map<std::string, Sprite*>&   GetAllSprites()   const { return sprites; }
    const std::unordered_map<std::string, Texture2D*>& GetAllTextures() const { return textures; }
    const std::unordered_map<std::string, Shader*>&   GetAllShaders()   const { return shaders; }

    void AddShader(Shader* shader);
    void AddTexture(Texture2D* texture);
    void AddMaterial(Material* material);
    void AddSprite(Sprite* sprite);

    void LoadAsset(const YAML::Node& node, const std::string& type);
    void LoadAssetFromFile(const std::string& filePath);

    // Scan a directory recursively for meta files (.texture, .shader, .material,
    // .mat, .sprite) and load any assets not already registered.
    void LoadFromDirectory(const std::string& rootDir);

    private:
    void LoadMaterial(const YAML::Node& node, const std::string& filePath = {});
    void LoadTexture (const YAML::Node& node, const std::string& filePath = {});
    void LoadShader  (const YAML::Node& node, const std::string& filePath = {});
    void LoadSprite  (const YAML::Node& node, const std::string& filePath = {});
    AssetManager() = default;

    std::unordered_map<std::string, Shader*> shaders;
    std::unordered_map<std::string, Texture2D*> textures;
    std::unordered_map<std::string, Material*> materials;
    std::unordered_map<std::string, Sprite*> sprites;
};
