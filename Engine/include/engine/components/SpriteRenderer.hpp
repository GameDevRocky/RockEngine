#pragma once
#include "engine/components/Component.hpp"
#include "engine/serialization/SerializableFactory.hpp"
#include "yaml-cpp/yaml.h"
#include "engine/rendering/core/Material.hpp"
#include "engine/rendering/core/Texture2D.hpp"
#include "engine/rendering/core/Sprite.hpp"
#include <glm/glm.hpp>

class SpriteRenderer : public Component
{
    public:

    static inline const Event MATERIAL_CHANGED_EVENT = SpriteRenderer::CreateEvent();
    static inline const Event SPRITE_CHANGED_EVENT = SpriteRenderer::CreateEvent();
    static inline const Event VISIBILITY_CHANGED_EVENT = SpriteRenderer::CreateEvent();
    static inline const Event COLOR_CHANGED_EVENT = SpriteRenderer::CreateEvent();
    static inline const Event FLIP_X_CHANGED_EVENT = SpriteRenderer::CreateEvent();
    static inline const Event FLIP_Y_CHANGED_EVENT = SpriteRenderer::CreateEvent();
    static inline const Event SORTING_LAYER_CHANGED_EVENT = SpriteRenderer::CreateEvent();
    static inline const Event SORTING_ORDER_CHANGED_EVENT = SpriteRenderer::CreateEvent();
    static inline const Event UV_SCALE_CHANGED_EVENT = SpriteRenderer::CreateEvent();
    static inline const Event UV_OFFSET_CHANGED_EVENT = SpriteRenderer::CreateEvent();



    SpriteRenderer* Copy() override;
    YAML::Node Serialize() override;

    void Deserialize(const YAML::Node& node) override;
    
    Material* GetMaterial();
    void SetMaterial(std::string& id);
    
    Sprite* GetSprite();
    void SetSprite(std::string& id);
    const std::string& GetSpriteID() const { return sprite_id; }
    
    void SetColor(const glm::vec4& c) { color = c; Notify(COLOR_CHANGED_EVENT); }
    glm::vec4 GetColor() const { return color; }
    
    void SetSortingLayer(const std::string& layer);
    const std::string& GetSortingLayer() const { return sortingLayer; }

    void SetSortingOrder(int order);
    int GetSortingOrder() const { return sortingOrder; }
    
    void SetVisible(bool& value);
    bool GetVisible(){return visible;};
    
    bool GetFlipX() const { return flipX;}
    bool GetFlipY() const { return flipY;}
    
    void SetFlipX(bool v) { flipX = v; Notify(FLIP_X_CHANGED_EVENT);}
    void SetFlipY(bool v) { flipY = v; Notify(FLIP_Y_CHANGED_EVENT);}

    glm::vec2 GetUVScale(){return this->uvScale;}
    glm::vec2 GetUVOffset(){return this->uvOffset;}

    void SetUVScale(glm::vec2 scale){uvScale = scale; Notify(UV_SCALE_CHANGED_EVENT);}
    void SetUVOffset(glm::vec2 offset){uvOffset = offset; Notify(UV_OFFSET_CHANGED_EVENT);}

    void SetUniformOverride(const std::string& name, const UniformValue& value);
    void RemoveUniformOverride(const std::string& name);

    void OverrideUniforms();
    void Accept(IVisitor* v) override;

    std::string GetTypeName() const override { return "SpriteRenderer"; }

private:
    std::string material_id = "m1";
    std::string sprite_id = "sprite1";
    glm::vec2 uvOffset = {0,0};
    glm::vec2 uvScale = {1, 1};
    glm::vec4 color = glm::vec4(1, 1, 1, 1);

    std::string sortingLayer = "Default";
    int sortingOrder = 0;
    bool flipX = false;
    bool flipY = false;
    bool visible = true;
    std::unordered_map<std::string, UniformValue> uniformOverrides;
};
