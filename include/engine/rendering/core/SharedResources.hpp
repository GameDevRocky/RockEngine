#pragma once
#include <string>
#include <unordered_map>
#include <memory>

#include "Shader.hpp"
#include "engine/rendering/core/Texture2D.hpp"
#include "engine/core/System.hpp"
#include <unordered_map>


class SharedResources : public System{
public:
    static SharedResources& Get()
    {
        static SharedResources instance;
        return instance;
    }

    void Init();

    Shader* GetShader(const std::string& name);
    Texture2D* GetTexture(const std::string& name);

    void AddShader(const std::string& name, Shader* shader);
    void AddTexture(const std::string& name, Texture2D* texture);


private:
    SharedResources() = default;

    std::unordered_map<std::string, Shader*> m_Shaders;
    std::unordered_map<std::string, Texture2D*> m_Textures;
};
