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

    // Segment pairs {a0,b0, a1,b1, ...} tracing the boundary between this
    // sprite's opaque and transparent pixels, in the sprite's own quad space:
    // pixels (== world units), CENTERED on the quad, y-up. Feed them through the
    // quad's local offset + the object's world matrix to get world space.
    //
    // Exists so a shadow caster can be the sprite's actual silhouette rather than
    // its bounding box -- light passes through transparent pixels. Segments are
    // axis-aligned pixel edges with collinear runs merged, and are deliberately
    // UNORDERED: the shadow rasterizer treats every segment independently, so
    // there is no need to trace a connected contour.
    //
    // Cached. The source image is decoded once (Texture2D drops its pixels after
    // upload) and the result is reused until the UVs, texture or threshold change.
    const std::vector<glm::vec2>& GetOpaqueOutline(float alphaThreshold = 0.5f);

    void Accept(IVisitor* v) override;

private:
    void RebuildOpaqueOutline(float alphaThreshold);

    std::string texture_id;
    std::string name;
    glm::vec2 uvMin;
    glm::vec2 uvMax;
    glm::vec2 pivot;

    // Outline cache + the inputs it was built from (see GetOpaqueOutline).
    std::vector<glm::vec2> outlineCache;
    bool      outlineValid       = false;
    glm::vec2 outlineUVMin{ 0.0f };
    glm::vec2 outlineUVMax{ 0.0f };
    std::string outlineTextureId;
    float     outlineThreshold   = -1.0f;
};