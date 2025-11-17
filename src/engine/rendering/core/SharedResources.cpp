#include "engine/rendering/core/SharedResources.hpp"
#include "engine/debug/Console.hpp"
#include <iostream>

void SharedResources::Init()
{
    m_Shaders["grid"] = Shader::LoadFromPath("src/engine/rendering/shaders/grid.vert","src/engine/rendering/shaders/grid.frag");
}

Shader* SharedResources::GetShader(const std::string& name)
{
    if (m_Shaders.find(name) != m_Shaders.end())
        return m_Shaders[name];
    std::cerr << "Shader not found: " << name << std::endl;
    return nullptr;
}

Texture2D* SharedResources::GetTexture(const std::string& name)
{
    if (m_Textures.find(name) != m_Textures.end())
        return m_Textures[name];
    std::cerr << "Texture not found: " << name << std::endl;
    return nullptr;
}


void SharedResources::AddShader(const std::string& name, Shader* shader){
    if (!shader)
    {
        std::cerr << "Cannot add null shader: " << name << std::endl;
        return;
    }
    m_Shaders[name] = shader;

}

void SharedResources::AddTexture(const std::string& name, Texture2D* texture){
    if (!texture)
    {
        std::cerr << "Cannot add null texture: " << name << std::endl;
        return;
    }
    m_Textures[name] = texture;

}