#pragma once
#include "engine/components/Component.hpp"
#include "engine/serialization/SerializableFactory.hpp"
#include "yaml-cpp/yaml.h"
#include "engine/rendering/core/Material.hpp"
#include <glm/glm.hpp>

class SpriteRenderer : public Component
{
public:
    // ---------------- Serialization ----------------
    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;
    void PostDeserialize() override;

    // ---------------- Component Info ----------------
    std::string GetTypeName() const override { return "SpriteRenderer"; }

    // ---------------- Material ----------------
    Material* GetMaterial() const { return material; }
    void SetMaterial(Material* mat) { material = mat; }

    // ---------------- Sprite Properties ----------------
    glm::vec4 GetColor() const { return color; }
    glm::vec4 GetUVRect() const { return uvRect; }
    int GetSortingOrder() const { return sortingOrder; }
    bool GetFlipX() const { return flipX; }
    bool GetFlipY() const { return flipY; }

    void SetColor(const glm::vec4& c) { color = c; }
    void SetUVRect(const glm::vec4& uv) { uvRect = uv; }
    void SetSortingOrder(int order) { sortingOrder = order; }
    void SetFlipX(bool v) { flipX = v; }
    void SetFlipY(bool v) { flipY = v; }

private:
    // MATERIAL now owns:
    // - Shader
    // - Texture
    // - Uniform values
    Material* material = nullptr;

    glm::vec4 uvRect = glm::vec4(0, 0, 1, 1);
    glm::vec4 color = glm::vec4(1, 1, 1, 1);

    int sortingOrder = 0;
    bool flipX = false;
    bool flipY = false;
};
