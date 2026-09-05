#include "engine/bindings/PythonBindings.hpp"
#include "engine/core/InputManager.hpp"
#include <iostream>
#include "Engine.hpp"


void BindInputManager(pybind11::module_& m) {
    pybind11::module_ input_module = m.def_submodule("input_module", "InputManagerBindings");
    
    input_module.def("is_key_down", [](int keycode) {      
        return inputManager->IsKeyDown(keycode);
    });

    input_module.def("is_key_pressed", [](int keycode) {       
        return inputManager->IsKeyPressed(keycode);
    });

    input_module.def("is_mouse_button_down", [](int button) {
        return inputManager->IsMouseButtonDown(button);
    });

    input_module.def("is_mouse_button_pressed", [](int button) {
        return inputManager->IsMouseButtonPressed(button);
    });

    // World space, resolved against the camera at call time -- correct on any frame, not only
    // frames where the mouse moved.
    input_module.def("get_mouse_pos", []() {
        glm::vec2 val = inputManager->GetMousePosition();
        return std::make_tuple(val.x, val.y);
    });

    // Panel pixels (DPI-scaled, top-left origin) -- for UI hit-testing that should not move
    // with the camera.
    input_module.def("get_mouse_screen_pos", []() {
        glm::vec2 val = inputManager->GetMouseScreenPosition();
        return std::make_tuple(val.x, val.y);
    });
}