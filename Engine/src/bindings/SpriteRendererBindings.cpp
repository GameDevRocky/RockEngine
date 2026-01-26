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

    m.def("set_flip", [](const std::string& id, bool x, bool y) {
        Engine* engine = Engine::Get();
        Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();
        GameObject* go = registry->Find<GameObject>(id); 
        if (go) {
            if (auto* renderer = go->GetComponent<SpriteRenderer>()) {
                renderer->SetFlipX(x);
                renderer->SetFlipY(y);
            }
        }
    });

    m.def("get_flip", [](const std::string& id) {
        Engine* engine = Engine::Get();
        Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();
        GameObject* go = registry->Find<GameObject>(id); 
        if (go) {
            if (auto* renderer = go->GetComponent<SpriteRenderer>()) {
                return std::make_tuple(renderer->GetFlipX(), renderer->GetFlipY());
            }
        }
        return std::make_tuple(false, false);
    });

    m.def("set_color", [](const std::string& id, float r, float g, float b, float a) {
        Engine* engine = Engine::Get();
        Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();
        GameObject* go = registry->Find<GameObject>(id);
        if (go) {
            if (auto* renderer = go->GetComponent<SpriteRenderer>()) {
                renderer->SetColor(glm::vec4(r, g, b, a));
            }
        }
    });

    m.def("get_color", [](const std::string& id) {
        Engine* engine = Engine::Get();
        Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();
        GameObject* go = registry->Find<GameObject>(id);
        if (go) {
            if (auto* renderer = go->GetComponent<SpriteRenderer>()) {
                const glm::vec4 c = renderer->GetColor();
                return std::make_tuple(c.r, c.g, c.b, c.a);
            }
        }
        return std::make_tuple(1.0f, 1.0f, 1.0f, 1.0f);
    });


    
}