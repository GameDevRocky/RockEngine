#include "engine/bindings/PythonBindings.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/rendering/core/Material.hpp"
#include "engine/rendering/core/Shader.hpp"

void BindMaterial(pybind11::module_& m) {
    pybind11::module_ material_module = m.def_submodule("material_module", "Material Bindings");

    material_module.def("get_name", [](const std::string& id) -> std::string {
        Material* mat = AssetManager::Get().GetMaterial(id);
        if (!mat) return {};
        return mat->GetName();
    });

    material_module.def("set_name", [](const std::string& id, std::string name) {
        Material* mat = AssetManager::Get().GetMaterial(id);
        if (mat) mat->SetName(name);
    });

    material_module.def("get_shader_id", [](const std::string& id) -> std::string {
        Material* mat = AssetManager::Get().GetMaterial(id);
        if (!mat) return {};
        Shader* shader = mat->GetShader();
        if (!shader) return {};
        return shader->GetID();
    });

    material_module.def("set_shader", [](const std::string& id, std::string shader_id) {
        Material* mat = AssetManager::Get().GetMaterial(id);
        if (mat) mat->SetShader(shader_id);
    });

    material_module.def("set_float", [](const std::string& id, const std::string& name, float value) {
        Material* mat = AssetManager::Get().GetMaterial(id);
        if (mat) mat->SetFloat(name, value);
    });

    material_module.def("set_vec2", [](const std::string& id, const std::string& name, float x, float y) {
        Material* mat = AssetManager::Get().GetMaterial(id);
        if (mat) mat->SetVec2(name, {x, y});
    });

    material_module.def("set_vec3", [](const std::string& id, const std::string& name, float x, float y, float z) {
        Material* mat = AssetManager::Get().GetMaterial(id);
        if (mat) mat->SetVec3(name, {x, y, z});
    });

    material_module.def("set_vec4", [](const std::string& id, const std::string& name, float x, float y, float z, float w) {
        Material* mat = AssetManager::Get().GetMaterial(id);
        if (mat) mat->SetVec4(name, {x, y, z, w});
    });

    material_module.def("set_texture", [](const std::string& id, const std::string& name, const std::string& tex_id) {
        Material* mat = AssetManager::Get().GetMaterial(id);
        if (mat) mat->SetTexture(name, tex_id);
    });
}
