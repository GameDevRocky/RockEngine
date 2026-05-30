#include "engine/bindings/PythonBindings.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include "engine/rendering/core/SharedResources.hpp"
#include "engine/rendering/core/Material.hpp"
#include "engine/rendering/core/Sprite.hpp"
#include "engine/rendering/core/Texture2D.hpp"
#include "engine/rendering/core/Shader.hpp"
#include "Engine.hpp"
#include <iostream>

void BindSpriteRenderer(pybind11::module_& m) {
    pybind11::module_ sprite_renderer_module = m.def_submodule("sprite_renderer_module", "Sprite Renderer Bindings");
    sprite_renderer_module.def("set_flip", [](const std::string& id, bool x, bool y) {
        GameObject* go = registry->Find<GameObject>(id); 
        if (go) {
            if (auto* renderer = go->GetComponent<SpriteRenderer>()) {
                renderer->SetFlipX(x);
                renderer->SetFlipY(y);
            }
        }
    });

    sprite_renderer_module.def("get_flip", [](const std::string& id) {
        GameObject* go = registry->Find<GameObject>(id); 
        if (go) {
            if (auto* renderer = go->GetComponent<SpriteRenderer>()) {
                return std::make_tuple(renderer->GetFlipX(), renderer->GetFlipY());
            }
        }
        return std::make_tuple(false, false);
    });

    sprite_renderer_module.def("set_color", [](const std::string& id, float r, float g, float b, float a) {
        GameObject* go = registry->Find<GameObject>(id);
        if (go) {
            if (auto* renderer = go->GetComponent<SpriteRenderer>()) {
                renderer->SetColor(glm::vec4(r, g, b, a));
            }
        }
    });

    sprite_renderer_module.def("get_color", [](const std::string& id) {
        GameObject* go = registry->Find<GameObject>(id);
        if (go) {
            if (auto* renderer = go->GetComponent<SpriteRenderer>()) {
                const glm::vec4 c = renderer->GetColor();
                return std::make_tuple(c.r, c.g, c.b, c.a);
            }
        }
        return std::make_tuple(1.0f, 1.0f, 1.0f, 1.0f);
    });

    sprite_renderer_module.def("set_uv_scale", [](const std::string& id, float x, float y) {
        GameObject* go = registry->Find<GameObject>(id);
        if (go) {
            if (auto* renderer = go->GetComponent<SpriteRenderer>()) {
                renderer->SetUVScale({x, y});
            }
        }
    });

    sprite_renderer_module.def("get_uv_scale", [](const std::string& id) {
        GameObject* go = registry->Find<GameObject>(id);
        if (go) {
            if (auto* renderer = go->GetComponent<SpriteRenderer>()) {
                const glm::vec2 scale = renderer->GetUVScale();
                return std::make_tuple(scale.x, scale.y);
            }
        }
        return std::make_tuple(1.0f, 1.0f);
    });

    sprite_renderer_module.def("set_uv_offset", [](const std::string& id, float x, float y) {
        GameObject* go = registry->Find<GameObject>(id);
        if (go) {
            if (auto* renderer = go->GetComponent<SpriteRenderer>()) {
                renderer->SetUVOffset({x, y});
            }
        }
    });

    sprite_renderer_module.def("get_uv_offset", [](const std::string& id) {
        GameObject* go = registry->Find<GameObject>(id);
        if (go) {
            if (auto* renderer = go->GetComponent<SpriteRenderer>()) {
                const glm::vec2 offset = renderer->GetUVOffset();
                return std::make_tuple(offset.x, offset.y);
            }
        }
        return std::make_tuple(1.0f, 1.0f);
    });

    sprite_renderer_module.def("get_sprite_id", [](const std::string& id) -> std::string {
        GameObject* go = registry->Find<GameObject>(id);
        if (go) {
            if (auto* renderer = go->GetComponent<SpriteRenderer>())
                return renderer->GetSpriteID();
        }
        return {};
    });

    sprite_renderer_module.def("set_sprite_id", [](const std::string& go_id, std::string sprite_id) {
        GameObject* go = registry->Find<GameObject>(go_id);
        if (go) {
            if (auto* renderer = go->GetComponent<SpriteRenderer>())
                renderer->SetSprite(sprite_id);
        }
    });

    sprite_renderer_module.def("set_uniform_float", [](const std::string& go_id, const std::string& name, float v) {
        GameObject* go = registry->Find<GameObject>(go_id);
        if (go) {
            if (auto* renderer = go->GetComponent<SpriteRenderer>())
                renderer->SetUniformOverride(name, v);
        }
    });

    sprite_renderer_module.def("set_uniform_vec2", [](const std::string& go_id, const std::string& name, float x, float y) {
        GameObject* go = registry->Find<GameObject>(go_id);
        if (go) {
            if (auto* renderer = go->GetComponent<SpriteRenderer>())
                renderer->SetUniformOverride(name, glm::vec2(x, y));
        }
    });

    sprite_renderer_module.def("set_uniform_vec3", [](const std::string& go_id, const std::string& name, float x, float y, float z) {
        GameObject* go = registry->Find<GameObject>(go_id);
        if (go) {
            if (auto* renderer = go->GetComponent<SpriteRenderer>())
                renderer->SetUniformOverride(name, glm::vec3(x, y, z));
        }
    });

    sprite_renderer_module.def("set_uniform_vec4", [](const std::string& go_id, const std::string& name, float x, float y, float z, float w) {
        GameObject* go = registry->Find<GameObject>(go_id);
        if (go) {
            if (auto* renderer = go->GetComponent<SpriteRenderer>())
                renderer->SetUniformOverride(name, glm::vec4(x, y, z, w));
        }
    });

    sprite_renderer_module.def("set_uniform_texture", [](const std::string& go_id, const std::string& name, const std::string& texture_id) {
        GameObject* go = registry->Find<GameObject>(go_id);
        if (go) {
            if (auto* renderer = go->GetComponent<SpriteRenderer>())
                renderer->SetUniformOverride(name, texture_id);
        }
    });

    sprite_renderer_module.def("remove_uniform", [](const std::string& go_id, const std::string& name) {
        GameObject* go = registry->Find<GameObject>(go_id);
        if (go) {
            if (auto* renderer = go->GetComponent<SpriteRenderer>())
                renderer->RemoveUniformOverride(name);
        }
    });

}