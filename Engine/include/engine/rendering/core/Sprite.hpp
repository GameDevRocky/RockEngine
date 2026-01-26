#pragma once
#include "engine/serialization/Serializable.hpp"
#include "engine/rendering/core/Texture2D.hpp"
#include <unordered_map>
#include <string>
#include <glm/glm.hpp>
#include "yaml-cpp/yaml.h"

class Sprite : public Serializable{

public:
    Sprite() = default;
    
    void Init(){}

    YAML::Node Serialize() override { return YAML::Node(); }
    void Deserialize(const YAML::Node& node) override;
    void PostDeserialize() override; 

    std::string GetTypeName() override {return "Sprite";};
    std::string GetName() {return name;}

    glm::vec2 GetUVMin() {return uvMin;}
    glm::vec2 GetUVMax() {return uvMax;}
    glm::vec2 GetPixelSize() {return pixelSize;}
    glm::vec2 GetPivot() {return pivot;}

    void SetUVMin(glm::vec2 min);
    void SetUVMax(glm::vec2 max);
    void SetPivot(glm::vec2 pivot);
    

    

    Texture2D* GetTexture();
    void SetTexture(std::string& id);

private:
    std::string texture_id;
    std::string name;
    glm::vec2 uvMin;
    glm::vec2 uvMax;
    glm::vec2 pivot;
    glm::vec2 pixelSize; 


};