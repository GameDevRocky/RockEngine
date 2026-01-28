#include "engine/bindings/PythonBindings.hpp"
#include "engine/core/InputManager.hpp"
#include <iostream>
#include "Engine.hpp"


void BindInputManager(pybind11::module_& m) {
    pybind11::module_ input_module = m.def_submodule("input_module", "InputManagerBindings");
    
    input_module.def("is_key_down", [](int keycode) {
        Engine* engine = Engine::Get();
        InputManager* inputManager = engine->GetActiveContainer()->FindSystem<InputManager>();
        return inputManager->IsKeyDown(keycode);
    });
    
    input_module.def("get_mouse_pos", []() {
        Engine* engine = Engine::Get();
        InputManager* inputManager = engine->GetActiveContainer()->FindSystem<InputManager>();
        glm::vec2 val = inputManager->GetMousePosition();
        // Returning a tuple is the most memory-efficient way for "light" data
        return std::make_tuple(val.x, val.y); 
    });
}