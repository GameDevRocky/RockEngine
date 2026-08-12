#include "engine/components/TextRenderer.hpp"

#include <algorithm>

#include "engine/components/Transform.hpp"
#include "engine/debug/Console.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/rendering/core/Font.hpp"
#include "engine/utils/EngineUtils.hpp"

using namespace EngineUtils;

// ─────────────────────────────────────────────────────────────────────────────
// Resolved by name at construction rather than baked in as literal ids, because
// the ids belong to the meta files (AssetMetaService regenerates them on a name
// collision) and would go stale. Best-effort: if the assets are not loaded yet,
// or the project has removed them, the ids stay empty -- exactly the state a
// deliberately cleared font produces, which the draw path already reads as
// "draw nothing" rather than as an error.
TextRenderer::TextRenderer() {
    if (Font* font = AssetManager::Get().GetFontByName(kDefaultFontName))
        font_id = font->GetID();
    if (Material* mat = AssetManager::Get().GetMaterialByName(kDefaultMaterialName))
        material_id = mat->GetID();
}

// ─────────────────────────────────────────────────────────────────────────────
YAML::Node TextRenderer::Serialize() {
    YAML::Node node = Component::Serialize();
    node["text"]        = text;
    node["font_id"]     = font_id;
    node["material_id"] = material_id;

    node["fontSize"]      = fontSize;
    node["lineSpacing"]   = lineSpacing;
    node["letterSpacing"] = letterSpacing;
    node["maxWidth"]      = maxWidth;
    node["hAlign"]        = static_cast<int>(hAlign);
    node["vAlign"]        = static_cast<int>(vAlign);

    node["color"][0] = color.r; node["color"][1] = color.g;
    node["color"][2] = color.b; node["color"][3] = color.a;
    node["color"].SetStyle(YAML::EmitterStyle::Flow);

    node["weight"] = weight;

    node["outlineColor"][0] = outlineColor.r; node["outlineColor"][1] = outlineColor.g;
    node["outlineColor"][2] = outlineColor.b; node["outlineColor"][3] = outlineColor.a;
    node["outlineColor"].SetStyle(YAML::EmitterStyle::Flow);
    node["outlineWidth"] = outlineWidth;

    node["visible"]      = visible;
    node["sortingLayer"] = sortingLayer;
    node["sortingOrder"] = sortingOrder;
    return node;
}

void TextRenderer::Deserialize(const YAML::Node& node) {
    Component::Deserialize(node);

    // Every field defaulted. A scene authored before a field existed must still
    // load, and yaml-cpp throws rather than returning a default on a missing key.
    text        = node["text"].as<std::string>("New Text");
    // Fall back to whatever the constructor resolved, not to empty: a scene
    // authored before these keys existed then loads with the default font and
    // text material rather than as an invisible component. A scene that stores
    // an empty id stored it deliberately (the font was cleared) and keeps it --
    // yaml-cpp round-trips an empty string as `""`, not as a missing key.
    font_id     = node["font_id"].as<std::string>(font_id);
    material_id = node["material_id"].as<std::string>(material_id);

    fontSize      = node["fontSize"].as<float>(32.0f);
    lineSpacing   = node["lineSpacing"].as<float>(1.0f);
    letterSpacing = node["letterSpacing"].as<float>(0.0f);
    maxWidth      = node["maxWidth"].as<float>(0.0f);
    hAlign = static_cast<TextHAlign>(node["hAlign"].as<int>(static_cast<int>(TextHAlign::Center)));
    vAlign = static_cast<TextVAlign>(node["vAlign"].as<int>(static_cast<int>(TextVAlign::Middle)));

    if (node["color"] && node["color"].size() == 4)
        color = glm::vec4(node["color"][0].as<float>(), node["color"][1].as<float>(),
                          node["color"][2].as<float>(), node["color"][3].as<float>());

    weight = node["weight"].as<float>(0.0f);

    if (node["outlineColor"] && node["outlineColor"].size() == 4)
        outlineColor = glm::vec4(node["outlineColor"][0].as<float>(), node["outlineColor"][1].as<float>(),
                                 node["outlineColor"][2].as<float>(), node["outlineColor"][3].as<float>());
    outlineWidth = node["outlineWidth"].as<float>(0.0f);

    visible      = node["visible"].as<bool>(true);
    sortingLayer = node["sortingLayer"].as<std::string>("Default");
    sortingOrder = node["sortingOrder"].as<int>(0);

    state = State::Loaded;
}

TextRenderer* TextRenderer::Copy() {
    TextRenderer* copy = new TextRenderer();
    // The id is copied deliberately, matching every other component. FontManager
    // keys its GPU mesh cache on it, so the editor component and its play-mode
    // copy share one entry -- correct, because the mesh is a pure function of the
    // state below, and the cache validates by content hash rather than trusting
    // a per-component dirty flag.
    copy->id            = id;
    copy->enabled       = enabled;
    copy->gameobject_id = gameobject_id;

    copy->text          = text;
    copy->font_id       = font_id;
    copy->material_id   = material_id;
    copy->fontSize      = fontSize;
    copy->lineSpacing   = lineSpacing;
    copy->letterSpacing = letterSpacing;
    copy->maxWidth      = maxWidth;
    copy->hAlign        = hAlign;
    copy->vAlign        = vAlign;
    copy->color         = color;
    copy->weight        = weight;
    copy->outlineColor  = outlineColor;
    copy->outlineWidth  = outlineWidth;
    copy->visible       = visible;
    copy->sortingLayer  = sortingLayer;
    copy->sortingOrder  = sortingOrder;
    copy->uniformOverrides = uniformOverrides;
    return copy;
}

// ─────────────────────────────────────────────────────────────────────────────
void TextRenderer::SetText(const std::string& v) {
    if (text == v) return;
    text = v;
    Notify(TEXT_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

Font* TextRenderer::GetFont() const {
    if (font_id.empty()) return nullptr;
    return AssetManager::Get().GetFont(font_id);
}

void TextRenderer::SetFont(const std::string& id) {
    if (font_id == id) return;
    // Unlike SpriteRenderer::SetMaterial this accepts an unknown id rather than
    // rejecting it: clearing the font (empty id) is a legitimate edit, and the
    // draw path already treats an unresolvable font as "draw nothing".
    font_id = id;
    Notify(FONT_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

TextLayoutSpec TextRenderer::GetLayoutSpec() const {
    TextLayoutSpec spec;
    spec.fontSize      = fontSize;
    spec.lineSpacing   = lineSpacing;
    spec.letterSpacing = letterSpacing;
    spec.maxWidth      = maxWidth;
    spec.hAlign        = hAlign;
    spec.vAlign        = vAlign;
    return spec;
}

void TextRenderer::SetFontSize(float v) {
    v = std::max(0.0f, v);
    if (fontSize == v) return;
    fontSize = v;
    Notify(FONT_SIZE_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void TextRenderer::SetLineSpacing(float v) {
    if (lineSpacing == v) return;
    lineSpacing = v;
    Notify(LINE_SPACING_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void TextRenderer::SetLetterSpacing(float v) {
    if (letterSpacing == v) return;
    letterSpacing = v;
    Notify(LETTER_SPACING_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void TextRenderer::SetMaxWidth(float v) {
    v = std::max(0.0f, v);
    if (maxWidth == v) return;
    maxWidth = v;
    Notify(MAX_WIDTH_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void TextRenderer::SetHAlign(TextHAlign v) {
    if (hAlign == v) return;
    hAlign = v;
    Notify(H_ALIGN_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void TextRenderer::SetVAlign(TextVAlign v) {
    if (vAlign == v) return;
    vAlign = v;
    Notify(V_ALIGN_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

// ─────────────────────────────────────────────────────────────────────────────
void TextRenderer::SetColor(const glm::vec4& v) {
    if (color == v) return;
    color = v;
    Notify(COLOR_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void TextRenderer::SetWeight(float v) {
    // Beyond about a quarter of the distance range the field runs out and the
    // glyph starts eroding rather than thickening.
    v = std::clamp(v, -0.25f, 0.25f);
    if (weight == v) return;
    weight = v;
    Notify(WEIGHT_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void TextRenderer::SetOutlineColor(const glm::vec4& v) {
    if (outlineColor == v) return;
    outlineColor = v;
    Notify(OUTLINE_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void TextRenderer::SetOutlineWidth(float v) {
    v = std::clamp(v, 0.0f, 0.35f);
    if (outlineWidth == v) return;
    outlineWidth = v;
    Notify(OUTLINE_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void TextRenderer::SetVisible(bool v) {
    if (visible == v) return;
    visible = v;
    Notify(VISIBILITY_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

// ─────────────────────────────────────────────────────────────────────────────
Material* TextRenderer::GetMaterial() const {
    if (material_id.empty()) return nullptr;
    return AssetManager::Get().GetMaterial(material_id);
}

void TextRenderer::SetMaterial(const std::string& id) {
    if (material_id == id) return;
    material_id = id;
    Notify(MATERIAL_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void TextRenderer::SetUniformOverride(const std::string& name, const UniformValue& value) {
    uniformOverrides[name] = value;
}

void TextRenderer::RemoveUniformOverride(const std::string& name) {
    uniformOverrides.erase(name);
}

void TextRenderer::OverrideUniforms(int firstFreeTextureSlot) {
    Material* mat = GetMaterial();
    if (!mat) return;
    Shader* shader = mat->GetShader();
    if (!shader) return;

    // ── Compatibility with the sprite quad contract ─────────────────────────
    // Every shader in the library assumes ScenePass's unit quad: aPos in
    // [-0.5,0.5] scaled by uSize, shifted by uPivot, with UVs remapped through
    // uUVScale/uUVOffset onto a sprite's atlas sub-rect. A glyph mesh arrives
    // already positioned in text-local units with final atlas UVs, so that whole
    // chain has to collapse to identity. Pushing these four unconditionally is
    // what lets an arbitrary material from the sprite library draw text with
    // correct geometry instead of folding the whole string into one unit.
    // Setting a uniform the shader doesn't declare is a no-op, so a purpose-built
    // text shader pays nothing for this.
    shader->SetVec2("uSize",     glm::vec2(1.0f));
    shader->SetVec2("uPivot",    glm::vec2(0.0f));
    shader->SetVec2("uUVScale",  glm::vec2(1.0f));
    shader->SetVec2("uUVOffset", glm::vec2(0.0f));

    // ── Component-owned uniforms ────────────────────────────────────────────
    // These are in Material's reserved list, so they never appear as editable
    // material properties -- a control the component always overrides would read
    // as broken.
    shader->SetVec4("uColor",        color);
    shader->SetFloat("uWeight",      weight);
    shader->SetVec4("uOutlineColor", outlineColor);
    shader->SetFloat("uOutlineWidth", outlineWidth);

    // ── Per-instance overrides ──────────────────────────────────────────────
    // The starting texture slot is handed in rather than assumed, because the
    // material has already consumed slots 0..n-1 for its own samplers and only
    // it knows how many resolved.
    int textureSlot = firstFreeTextureSlot;
    for (const auto& [uName, uValue] : uniformOverrides) {
        std::visit([&](const auto& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, float>)
                shader->SetFloat(uName, v);
            else if constexpr (std::is_same_v<T, glm::vec2>)
                shader->SetVec2(uName, v);
            else if constexpr (std::is_same_v<T, glm::vec3>)
                shader->SetVec3(uName, v);
            else if constexpr (std::is_same_v<T, glm::vec4>)
                shader->SetVec4(uName, v);
            else if constexpr (std::is_same_v<T, std::string>) {
                if (Texture2D* tex = AssetManager::Get().GetTexture(v)) {
                    tex->Bind(textureSlot);
                    shader->SetTexture(uName, textureSlot++);
                }
            }
        }, uValue);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void TextRenderer::SetSortingLayer(const std::string& layer) {
    if (sortingLayer == layer) return;
    sortingLayer = layer;
    Notify(SORTING_LAYER_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void TextRenderer::SetSortingOrder(int order) {
    if (sortingOrder == order) return;
    sortingOrder = order;
    Notify(SORTING_ORDER_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void TextRenderer::Accept(IVisitor* v) { v->Visit(this); }
