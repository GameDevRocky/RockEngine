#include "engine/bindings/PythonBindings.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/rendering/core/Texture2D.hpp"
#include "Engine.hpp"

// Texture2D is a rendering asset (lives in AssetManager, not the registry), so
// functions are keyed by the texture's asset id -- mirroring the Sprite/Material
// bindings. Filter/wrap enums cross as ints.
void BindTexture2D(pybind11::module_& m) {
    pybind11::module_ texture_module = m.def_submodule("texture_module", "Texture2D Bindings");

    texture_module.def("get_width", [](const std::string& id) {
        Texture2D* tex = AssetManager::Get().GetTexture(id);
        return tex ? tex->GetWidth() : 0;
    });

    texture_module.def("get_height", [](const std::string& id) {
        Texture2D* tex = AssetManager::Get().GetTexture(id);
        return tex ? tex->GetHeight() : 0;
    });

    texture_module.def("get_path", [](const std::string& id) -> std::string {
        Texture2D* tex = AssetManager::Get().GetTexture(id);
        return tex ? tex->GetPath() : std::string{};
    });

    // OpenGL texture handle -- useful for interop/debugging.
    texture_module.def("get_texture_id", [](const std::string& id) {
        Texture2D* tex = AssetManager::Get().GetTexture(id);
        return tex ? static_cast<unsigned int>(tex->GetTextureID()) : 0u;
    });

    texture_module.def("get_filter", [](const std::string& id) {
        Texture2D* tex = AssetManager::Get().GetTexture(id);
        return tex ? static_cast<int>(tex->GetFilter()) : 0;
    });

    texture_module.def("set_filter", [](const std::string& id, int filter) {
        Texture2D* tex = AssetManager::Get().GetTexture(id);
        if (tex) tex->SetFilter(static_cast<TextureFilter>(filter));
    });

    texture_module.def("get_wrap", [](const std::string& id) {
        Texture2D* tex = AssetManager::Get().GetTexture(id);
        return tex ? static_cast<int>(tex->GetWrap()) : 0;
    });

    texture_module.def("set_wrap", [](const std::string& id, int wrap) {
        Texture2D* tex = AssetManager::Get().GetTexture(id);
        if (tex) tex->SetWrap(static_cast<TextureWrap>(wrap));
    });
}
