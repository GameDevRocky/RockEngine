#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "engine/rendering/core/Texture2D.hpp"
#include <iostream>


Texture2D::~Texture2D()
{
    if (texture_id)
        glDeleteTextures(1, &texture_id);
}


void Texture2D::ApplySettings() const
{
    glBindTexture(GL_TEXTURE_2D, texture_id);
    
    // Change GL_LINEAR_MIPMAP_LINEAR to just GL_LINEAR
    GLenum minFilter = (filter == TextureFilter::Linear) ? GL_LINEAR : GL_NEAREST;
    GLenum magFilter = (filter == TextureFilter::Linear) ? GL_LINEAR : GL_NEAREST;
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
    
    GLenum wrapMode = (wrap == TextureWrap::Repeat) ? GL_REPEAT : GL_CLAMP_TO_EDGE;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapMode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapMode);
}

void Texture2D::Awake(){
    stbi_set_flip_vertically_on_load(true);

    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
    if (!data)
    {
        std::cerr << "Failed to load texture: " << path << std::endl;
        return;
    }

    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    GLenum format = channels == 4 ? GL_RGBA : GL_RGB;

    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);

    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    ApplySettings();
    stbi_image_free(data);
}

void Texture2D::Deserialize(const YAML::Node &node){
    Serializable::Deserialize(node);
    name = node["name"].as<std::string>();
    path = node["path"].as<std::string>();
    filter = node["filtering"].as<std::string>() == "linear" ? TextureFilter::Linear : TextureFilter::Nearest;
    wrap = node["wrap"].as<std::string>() == "clamp" ? TextureWrap::Clamp : TextureWrap::Repeat;
}


void Texture2D::Bind(unsigned int slot) const
{
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, texture_id);
}

void Texture2D::Unbind() const
{
    glBindTexture(GL_TEXTURE_2D, 0);
}
