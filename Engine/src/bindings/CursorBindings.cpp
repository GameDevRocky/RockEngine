#include "engine/bindings/PythonBindings.hpp"
#include "engine/input/CursorManager.hpp"

#include <pybind11/pybind11.h>

void BindCursor(pybind11::module_& m) {
    namespace py = pybind11;
    py::module_ cursor = m.def_submodule("cursor_module", "Cursor bindings");

    cursor.def("is_visible", []() { return CursorManager::Get().IsVisible(); });
    cursor.def("set_visible", [](bool value) { CursorManager::Get().SetVisible(value); });
    cursor.def("get_lock_state", []() {
        return static_cast<int>(CursorManager::Get().GetLockState());
    });
    cursor.def("set_lock_state", [](int value) {
        if (value < static_cast<int>(CursorLockState::None) ||
            value > static_cast<int>(CursorLockState::Locked))
            throw py::value_error("invalid CursorLockState");
        CursorManager::Get().SetLockState(static_cast<CursorLockState>(value));
    });
    cursor.def("set_sprite", [](const std::string& id, float hotX, float hotY) {
        if (!CursorManager::Get().SetSprite(id, {hotX, hotY}))
            throw py::value_error("unknown cursor sprite id: " + id);
    });
    cursor.def("reset_sprite", []() { CursorManager::Get().ResetSprite(); });
    cursor.def("get_sprite_id", []() { return CursorManager::Get().GetSpriteID(); });
    cursor.def("get_hotspot", []() {
        const glm::vec2 value = CursorManager::Get().GetHotspot();
        return py::make_tuple(value.x, value.y);
    });
    cursor.def("reset", []() { CursorManager::Get().Reset(); });
}
