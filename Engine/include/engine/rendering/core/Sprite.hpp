#pragma once
#include "engine/serialization/Serializable.hpp"
#include "engine/rendering/core/Resource.hpp"
#include <unordered_map>
#include <string>
#include <glm/glm.hpp>
#include "yaml-cpp/yaml.h"

class Texture2D;

class Sprite : public Resource {

public:

    static inline const Event UV_MIN_CHANGED_EVENT     = Sprite::CreateEvent();
    static inline const Event UV_MAX_CHANGED_EVENT     = Sprite::CreateEvent();
    static inline const Event PIVOT_CHANGED_EVENT      = Sprite::CreateEvent();
    static inline const Event TEXTURE_CHANGED_EVENT    = Sprite::CreateEvent();


    Sprite() = default;
    
    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;
    void Awake() override;

    std::string GetTypeName() override {return "Sprite";};

    glm::vec2 GetUVMin() {return uvMin;}
    glm::vec2 GetUVMax() {return uvMax;}
    glm::vec2 GetPixelSize();
    glm::vec2 GetPivot() {return pivot;}

    void SetUVMin(glm::vec2 min);
    void SetUVMax(glm::vec2 max);
    void SetPivot(glm::vec2 pivot);
    
    const std::string& GetTextureID() const { return texture_id; }
    Texture2D* GetTexture();
    void SetTexture(std::string& id);

    void Accept(IVisitor* v) override;

private:
    std::string texture_id;
    std::string name;
    glm::vec2 uvMin;
    glm::vec2 uvMax;
    glm::vec2 pivot;


};