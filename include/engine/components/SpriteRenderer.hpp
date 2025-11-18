#pragma once
#include "engine/components/Component.hpp"
#include "engine/serialization/SerializableFactory.hpp"
#include "yaml-cpp/yaml.h"
#include "engine/rendering/core/Texture2D.hpp"
#include <glm/glm.hpp>


class SpriteRenderer : public Component{

public:
    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;
    void PostDeserialize() override;
    std::string GetTypeName() const override { return "SpriteRenderer"; }
    Texture2D* GetTexture(){return texture;}

private:
    Texture2D* texture;
    glm::vec4 uvRect = glm::vec4(0, 0, 1, 1);
    int sortingOrder = 0;
    bool flipX = false;
    bool flipY = false;
};

// Registration moved to a single compiled TU: src/engine/components/ComponentRegistrars.cpp
