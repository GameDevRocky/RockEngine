#pragma once
#include <string>
#include <glad/glad.h>
#include "engine/serialization/Serializable.hpp"
#include <yaml-cpp/yaml.h>


enum class TextureFilter {
    Nearest,
    Linear
};

enum class TextureWrap {
    Repeat,
    Clamp
};

class Texture2D : public Serializable{
public:
    Texture2D() = default;

    ~Texture2D();

    void Bind(unsigned int slot = 0) const;
    void Unbind() const;
    void Init(){};
    void ApplySettings() const;
    YAML::Node Serialize() override { return YAML::Node(); }

    void Deserialize(const YAML::Node& node) override;
    void PostDeserialize() override;
    std::string GetTypeName() {return "Texture2D";};
    std::string GetName() {return name;};

    int GetWidth() const { return width; }
    int GetHeight() const { return height; }

    std::string GetPath() const {return path;}

    GLuint GetTextureID() const { return texture_id; }

private:
    GLuint texture_id = 0;
    int width = 0;
    int height = 0;
    int channels = 0;
    std::string path;
    std::string name;
    TextureFilter filter = TextureFilter::Linear;
    TextureWrap wrap = TextureWrap::Clamp;
};
