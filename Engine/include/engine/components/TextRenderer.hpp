#pragma once
#include <string>
#include <unordered_map>

#include <glm/glm.hpp>
#include <yaml-cpp/yaml.h>

#include "engine/components/Component.hpp"
#include "engine/rendering/core/Material.hpp"
#include "engine/serialization/SerializableFactory.hpp"
#include "engine/text/TextLayout.hpp"

class Font;

// World-space text as an ordinary GameObject component.
//
// Sits alongside SpriteRenderer in every way that matters: it needs a Transform
// on the same object, it carries a sorting layer + order so it interleaves with
// sprites and particles in ScenePass's single sorted draw list, and it draws
// through an assignable Material with per-instance uniform overrides.
//
// What it does NOT own is the GPU buffer holding its glyph quads. That lives in
// FontManager, keyed by this component's id, because play mode deep-copies the
// editor world and a GL handle must not be duplicated or destroyed along with
// the copy. Same reasoning as ParticleComponent and ParticleManager.
class TextRenderer : public Component {
public:
    // Asset NAMES (not ids) of what a freshly created TextRenderer starts with,
    // so a component dropped on an object draws something immediately instead of
    // rendering nothing until a font is picked. Names rather than ids because the
    // ids live in the meta files and are regenerated on collision.
    static constexpr const char* kDefaultFontName     = "default";  // Domain/lib/assets/fonts/default.ttf
    static constexpr const char* kDefaultMaterialName = "text";     // Domain/lib/assets/defaults/text.material

    TextRenderer();

    // ── Layout-affecting: these invalidate the cached glyph mesh ────────────
    static inline const Event TEXT_CHANGED_EVENT           = TextRenderer::CreateEvent();
    static inline const Event FONT_CHANGED_EVENT           = TextRenderer::CreateEvent();
    static inline const Event FONT_SIZE_CHANGED_EVENT      = TextRenderer::CreateEvent();
    static inline const Event LINE_SPACING_CHANGED_EVENT   = TextRenderer::CreateEvent();
    static inline const Event LETTER_SPACING_CHANGED_EVENT = TextRenderer::CreateEvent();
    static inline const Event MAX_WIDTH_CHANGED_EVENT      = TextRenderer::CreateEvent();
    static inline const Event H_ALIGN_CHANGED_EVENT        = TextRenderer::CreateEvent();
    static inline const Event V_ALIGN_CHANGED_EVENT        = TextRenderer::CreateEvent();

    // ── Uniform-only: cheap, must NOT rebuild the mesh ─────────────────────
    static inline const Event COLOR_CHANGED_EVENT          = TextRenderer::CreateEvent();
    static inline const Event WEIGHT_CHANGED_EVENT         = TextRenderer::CreateEvent();
    static inline const Event OUTLINE_CHANGED_EVENT        = TextRenderer::CreateEvent();
    static inline const Event MATERIAL_CHANGED_EVENT       = TextRenderer::CreateEvent();
    static inline const Event VISIBILITY_CHANGED_EVENT     = TextRenderer::CreateEvent();
    static inline const Event SORTING_LAYER_CHANGED_EVENT  = TextRenderer::CreateEvent();
    static inline const Event SORTING_ORDER_CHANGED_EVENT  = TextRenderer::CreateEvent();

    TextRenderer* Copy() override;
    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;

    std::string GetTypeName() const override { return "TextRenderer"; }
    void Accept(IVisitor* v) override;

    // ── Content ────────────────────────────────────────────────────────────
    const std::string& GetText() const { return text; }
    void SetText(const std::string& v);

    Font* GetFont() const;
    const std::string& GetFontID() const { return font_id; }
    void SetFont(const std::string& id);

    // ── Layout ─────────────────────────────────────────────────────────────
    float GetFontSize() const { return fontSize; }
    void  SetFontSize(float v);

    float GetLineSpacing() const { return lineSpacing; }
    void  SetLineSpacing(float v);

    float GetLetterSpacing() const { return letterSpacing; }
    void  SetLetterSpacing(float v);

    float GetMaxWidth() const { return maxWidth; }
    void  SetMaxWidth(float v);

    TextHAlign GetHAlign() const { return hAlign; }
    void       SetHAlign(TextHAlign v);

    TextVAlign GetVAlign() const { return vAlign; }
    void       SetVAlign(TextVAlign v);

    // Everything layout depends on, in one struct -- the input to TextLayout and
    // the thing FontManager hashes to decide whether its cached mesh is stale.
    TextLayoutSpec GetLayoutSpec() const;

    // ── Appearance ─────────────────────────────────────────────────────────
    glm::vec4 GetColor() const { return color; }
    void      SetColor(const glm::vec4& v);

    float GetWeight() const { return weight; }
    void  SetWeight(float v);

    glm::vec4 GetOutlineColor() const { return outlineColor; }
    void      SetOutlineColor(const glm::vec4& v);

    float GetOutlineWidth() const { return outlineWidth; }
    void  SetOutlineWidth(float v);

    bool GetVisible() const { return visible; }
    void SetVisible(bool v);

    // ── Material ───────────────────────────────────────────────────────────
    Material* GetMaterial() const;
    const std::string& GetMaterialID() const { return material_id; }
    void SetMaterial(const std::string& id);

    void SetUniformOverride(const std::string& name, const UniformValue& value);
    void RemoveUniformOverride(const std::string& name);
    const std::unordered_map<std::string, UniformValue>& GetUniformOverrides() const {
        return uniformOverrides;
    }

    // Pushes this component's own uniforms after the material's, and returns
    // nothing -- see the comment in the .cpp for why the texture slot has to be
    // handed in rather than assumed.
    void OverrideUniforms(int firstFreeTextureSlot);

    // ── Sorting ────────────────────────────────────────────────────────────
    const std::string& GetSortingLayer() const { return sortingLayer; }
    void SetSortingLayer(const std::string& layer);

    int  GetSortingOrder() const { return sortingOrder; }
    void SetSortingOrder(int order);

private:
    std::string text = "New Text";
    std::string font_id;
    std::string material_id;

    float fontSize      = 32.0f;
    float lineSpacing   = 1.0f;
    float letterSpacing = 0.0f;
    float maxWidth      = 0.0f;
    TextHAlign hAlign = TextHAlign::Center;
    TextVAlign vAlign = TextVAlign::Middle;

    glm::vec4 color        = glm::vec4(1.0f);
    float     weight       = 0.0f;
    glm::vec4 outlineColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    float     outlineWidth = 0.0f;

    bool visible = true;
    std::string sortingLayer = "Default";
    int sortingOrder = 0;

    std::unordered_map<std::string, UniformValue> uniformOverrides;
};
