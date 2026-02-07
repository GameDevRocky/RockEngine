#pragma once
#include <string>
#include <unordered_map>
#include <yaml-cpp/yaml.h>

#include "engine/core/System.hpp"
#include "engine/rendering/core/Shader.hpp"
#include "engine/rendering/core/Texture2D.hpp"
#include "engine/rendering/core/Material.hpp"
#include "engine/rendering/core/Sprite.hpp"

class SharedResources : public System
{
public:
    static SharedResources& Get()
    {
        static SharedResources instance;
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

    void AddShader(Shader* shader);
    void AddTexture(Texture2D* texture);
    void AddMaterial(Material* material);
    void AddSprite(Sprite* sprite);

private:
    SharedResources() = default;

    std::unordered_map<std::string, Shader*> shaders;
    std::unordered_map<std::string, Texture2D*> textures;
    std::unordered_map<std::string, Material*> materials;
    std::unordered_map<std::string, Sprite*> sprites;
};
