#pragma once
#include <string>
#include <vector>
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
    static inline const Event FILTER_CHANGED_EVENT = Texture2D::CreateEvent();
    static inline const Event WRAP_CHANGED_EVENT   = Texture2D::CreateEvent();
    // Fired from the destructor (payload = id) so UI bound to this texture (e.g. the
    // sprite editor modal) can close itself when the texture is destroyed in memory.
    static inline const Event DESTROYED_EVENT      = Texture2D::CreateEvent();

    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;

    // Records a sprite id in load order so re-saving keeps sprites stable/ordered.
    void RegisterSprite(const std::string& spriteId);

    // Removes a sprite id from this texture's list (used when a sprite is deleted).
    void UnregisterSprite(const std::string& spriteId);

    void Awake() override;

    void Bind(unsigned int slot = 0) const;
    void Unbind() const;

    void ApplySettings() const;

    std::string GetTypeName() {return "Texture2D";};

    void Accept(IVisitor* v) override;

    int GetWidth() const { return width; }
    int GetHeight() const { return height; }

    std::string GetPath() const { return path; }

    TextureFilter GetFilter() const { return filter; }
    TextureWrap   GetWrap()   const { return wrap; }

    void SetFilter(TextureFilter f);
    void SetWrap(TextureWrap w);

    GLuint GetTextureID() const { return texture_id; }

    Texture2D() = default;
    ~Texture2D();

private:
    GLuint texture_id = 0;
    int width = 0;
    int height = 0;
    int channels = 0;
    std::string path;
    std::vector<std::string> sprite_ids;
    TextureFilter filter = TextureFilter::Linear;
    TextureWrap wrap = TextureWrap::Clamp;
};
