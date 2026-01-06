#include "engine/bindings/PythonBindings.hpp"
#include "engine/core/InputManager.hpp"
#include <iostream>
void BindInput(pybind11::module_& m) {

    m.def("is_key_down", [](int keycode) {
        return InputManager::Get().IsKeyDown(keycode);
    });
    m.def("get_mouse_pos", []() {
    glm::vec2 val = InputManager::Get().GetMousePosition();
    // Returning a tuple is the most memory-efficient way for "light" data
    return std::make_tuple(val.x, val.y); 
    });
}