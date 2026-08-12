#include "engine/bindings/PythonBindings.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/components/TextRenderer.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/rendering/core/Font.hpp"
#include "Engine.hpp"

// Scripting surface for world-space text. Keep in sync with the Python handler in
// Domain/lib/api/components/text_renderer_handler.py.
//
// Deliberately narrow: the things a game actually changes at runtime -- the
// string, the colour, the size. Layout and sorting are authored in the editor and
// exposed here only where a script plausibly drives them.
void BindTextRenderer(pybind11::module_& m) {
    pybind11::module_ text_module = m.def_submodule("text_renderer_module", "Text Renderer Bindings");

    // Every entry point resolves the GameObject by id and no-ops when the object
    // or the component is gone -- a script holding a stale id must not crash the
    // interpreter, same convention as the other component bindings.
    auto find = [](const std::string& id) -> TextRenderer* {
        GameObject* go = registry->Find<GameObject>(id);
        return go ? go->GetComponent<TextRenderer>() : nullptr;
    };

    text_module.def("set_text", [find](const std::string& id, const std::string& value) {
        if (auto* t = find(id)) t->SetText(value);
    });

    text_module.def("get_text", [find](const std::string& id) {
        auto* t = find(id);
        return t ? t->GetText() : std::string();
    });

    text_module.def("set_color", [find](const std::string& id, float r, float g, float b, float a) {
        if (auto* t = find(id)) t->SetColor(glm::vec4(r, g, b, a));
    });

    text_module.def("get_color", [find](const std::string& id) {
        if (auto* t = find(id)) {
            const glm::vec4 c = t->GetColor();
            return std::make_tuple(c.r, c.g, c.b, c.a);
        }
        return std::make_tuple(1.0f, 1.0f, 1.0f, 1.0f);
    });

    text_module.def("set_font_size", [find](const std::string& id, float size) {
        if (auto* t = find(id)) t->SetFontSize(size);
    });

    text_module.def("get_font_size", [find](const std::string& id) {
        auto* t = find(id);
        return t ? t->GetFontSize() : 0.0f;
    });

    // Takes a font NAME rather than an id: a script author knows "Nunito", not the
    // UUID in the .font meta. Unknown names are ignored rather than clearing the
    // font, so a typo leaves the text readable instead of making it vanish.
    text_module.def("set_font", [find](const std::string& id, const std::string& fontName) {
        auto* t = find(id);
        if (!t) return;
        if (Font* font = AssetManager::Get().GetFontByName(fontName))
            t->SetFont(font->GetID());
    });

    text_module.def("get_font", [find](const std::string& id) {
        auto* t = find(id);
        if (!t) return std::string();
        Font* font = t->GetFont();
        return font ? font->GetName() : std::string();
    });

    text_module.def("set_visible", [find](const std::string& id, bool visible) {
        if (auto* t = find(id)) t->SetVisible(visible);
    });

    text_module.def("get_visible", [find](const std::string& id) {
        auto* t = find(id);
        return t ? t->GetVisible() : false;
    });

    // Alignment as plain ints, matching the TextHAlign / TextVAlign enum order.
    // The Python handler wraps these in readable constants.
    text_module.def("set_alignment", [find](const std::string& id, int h, int v) {
        if (auto* t = find(id)) {
            t->SetHAlign(static_cast<TextHAlign>(h));
            t->SetVAlign(static_cast<TextVAlign>(v));
        }
    });

    text_module.def("get_alignment", [find](const std::string& id) {
        if (auto* t = find(id))
            return std::make_tuple(static_cast<int>(t->GetHAlign()),
                                   static_cast<int>(t->GetVAlign()));
        return std::make_tuple(0, 0);
    });

    text_module.def("set_max_width", [find](const std::string& id, float width) {
        if (auto* t = find(id)) t->SetMaxWidth(width);
    });

    text_module.def("set_outline", [find](const std::string& id,
                                          float r, float g, float b, float a, float width) {
        if (auto* t = find(id)) {
            t->SetOutlineColor(glm::vec4(r, g, b, a));
            t->SetOutlineWidth(width);
        }
    });

    text_module.def("set_weight", [find](const std::string& id, float weight) {
        if (auto* t = find(id)) t->SetWeight(weight);
    });

    text_module.def("set_sorting_order", [find](const std::string& id, int order) {
        if (auto* t = find(id)) t->SetSortingOrder(order);
    });

    text_module.def("set_sorting_layer", [find](const std::string& id, const std::string& layer) {
        if (auto* t = find(id)) t->SetSortingLayer(layer);
    });
}
