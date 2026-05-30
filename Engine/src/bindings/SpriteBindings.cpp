#include "engine/bindings/PythonBindings.hpp"
#include "engine/rendering/core/SharedResources.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/components/Transform.hpp"
#include "Engine.hpp"

void BindSprite(pybind11::module_& m) {
    pybind11::module_ sprite_module = m.def_submodule("sprite_module", "Sprite Bindings");
    
    sprite_module.def("set_uv_min", [](const std::string& id, float x, float y) {
        Sprite* sprite = SharedResources::Get().GetSprite(id);
        if (!sprite) return;
        sprite->SetUVMin({x, y});
    });

    
    sprite_module.def("get_uv_min", [](const std::string& id) {
        Sprite* sprite = SharedResources::Get().GetSprite(id);
        if (!sprite) return std::make_tuple(0.0f, 0.0f);
        auto min = sprite->GetUVMin();
        return std::make_tuple(min.x, min.y);
    });
    
    sprite_module.def("set_uv_max", [](const std::string& id, float x, float y) {
        Sprite* sprite = SharedResources::Get().GetSprite(id);
        if (!sprite) return;
        sprite->SetUVMax({x, y});
    });
    
    sprite_module.def("get_uv_max", [](const std::string& id) {
        Sprite* sprite = SharedResources::Get().GetSprite(id);
        if (!sprite) return std::make_tuple(0.0f, 0.0f);
        auto max = sprite->GetUVMax();
        return std::make_tuple(max.x, max.y);
    });

    sprite_module.def("set_pivot", [](const std::string& id, float x, float y) {
        Sprite* sprite = SharedResources::Get().GetSprite(id);
        if (!sprite) return;
        sprite->SetPivot({x, y});
    });

    sprite_module.def("get_pivot", [](const std::string& id) {
        Sprite* sprite = SharedResources::Get().GetSprite(id);
        if (!sprite) return std::make_tuple(0.0f, 0.0f);
        auto pivot = sprite->GetPivot();
        return std::make_tuple(pivot.x, pivot.y);
    });
}