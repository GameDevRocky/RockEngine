#include "engine/bindings/PythonBindings.hpp"
#include "engine/core/InputManager.hpp"
#include <iostream>
#include "Engine.hpp"


void BindInput(pybind11::module_& m) {

    m.def("is_key_down", [](int keycode) {
        Engine* engine = Engine::Get();
        InputManager* inputManager = engine->GetActiveContainer()->FindSystem<InputManager>();
        return inputManager->IsKeyDown(keycode);
    });
    m.def("get_mouse_pos", []() {
        Engine* engine = Engine::Get();
        InputManager* inputManager = engine->GetActiveContainer()->FindSystem<InputManager>();
        glm::vec2 val = inputManager->GetMousePosition();
        // Returning a tuple is the most memory-efficient way for "light" data
        return std::make_tuple(val.x, val.y); 
    });
}