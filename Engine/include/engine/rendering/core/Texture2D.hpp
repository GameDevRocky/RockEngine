#pragma once
#include <string>
#include <glad/glad.h>
#include "engine/serialization/Serializable.hpp"
#include "engine/rendering/core/Resource.hpp"
#include <yaml-cpp/yaml.h>


enum class TextureFilter {
    Nearest,
    Linear
};

enum class TextureWrap {
    Repeat,
    Clamp
};

class Texture2D : public Resource {
public:
    
    void Deserialize(const YAML::Node& node) override;

    void Awake() override;

    void Bind(unsigned int slot = 0) const;
    void Unbind() const;

    void ApplySettings() const;

    std::string GetTypeName() {return "Texture2D";};

    int GetWidth() const { return width; }
    int GetHeight() const { return height; }

    std::string GetPath() const {return path;}

    GLuint GetTextureID() const { return texture_id; }

    Texture2D() = default;
    ~Texture2D();

private:
    GLuint texture_id = 0;
    int width = 0;
    int height = 0;
    int channels = 0;
    std::string path;
    TextureFilter filter = TextureFilter::Linear;
    TextureWrap wrap = TextureWrap::Clamp;
};
