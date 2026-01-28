#pragma once
#include <pybind11/pybind11.h>
// SYSTEMS
void BindInputManager(pybind11::module_& m);


void BindComponent(pybind11::module_& m);
void BindGameObject(pybind11::module_& m);
void BindTransform(pybind11::module_& m);
void BindSpriteRenderer(pybind11::module_& m);
namespace engine {
    void RegisterPythonBindings(); 
}