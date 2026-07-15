#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "engine/rendering/core/Texture2D.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/rendering/core/Sprite.hpp"
#include <iostream>
#include <algorithm>
#include "engine/utils/EngineUtils.hpp"

using namespace EngineUtils;

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
    Resource::Deserialize(node);
    path = GetAssetPath(node["path"].as<std::string>());
    filter = node["filtering"].as<std::string>() == "linear" ? TextureFilter::Linear : TextureFilter::Nearest;
    wrap = node["wrap"].as<std::string>() == "clamp" ? TextureWrap::Clamp : TextureWrap::Repeat;
}

void Texture2D::RegisterSprite(const std::string& spriteId){
    if (std::find(sprite_ids.begin(), sprite_ids.end(), spriteId) == sprite_ids.end())
        sprite_ids.push_back(spriteId);
}

YAML::Node Texture2D::Serialize(){
    YAML::Node node;
    node["type"] = GetTypeName();
    node["id"] = GetID();
    node["name"] = GetName();
    node["path"] = ToAssetRelative(path);
    node["filtering"] = filter == TextureFilter::Linear ? "linear" : "nearest";
    node["wrap"] = wrap == TextureWrap::Clamp ? "clamp" : "repeat";

    // Sprites live inside their texture's meta file. Emit those recorded in load
    // order first, then any others pointing at this texture (so none are dropped).
    YAML::Node sprites(YAML::NodeType::Sequence);
    std::vector<std::string> emitted;
    auto emit = [&](Sprite* s){
        if (!s) return;
        sprites.push_back(s->Serialize());
        emitted.push_back(s->GetID());
    };
    for (const auto& sid : sprite_ids)
        emit(AssetManager::Get().GetSprite(sid));
    for (const auto& kv : AssetManager::Get().GetAllSprites()) {
        Sprite* s = kv.second;
        if (!s || s->GetTextureID() != GetID()) continue;
        if (std::find(emitted.begin(), emitted.end(), s->GetID()) != emitted.end()) continue;
        emit(s);
    }
    if (sprites.size() > 0) node["sprites"] = sprites;
    return node;
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

void Texture2D::SetFilter(TextureFilter f) {
    filter = f;
    if (texture_id) ApplySettings();
    Notify(FILTER_CHANGED_EVENT);
}

void Texture2D::SetWrap(TextureWrap w) {
    wrap = w;
    if (texture_id) ApplySettings();
    Notify(WRAP_CHANGED_EVENT);
}

void Texture2D::Accept(IVisitor* v) { v->Visit(this); }
