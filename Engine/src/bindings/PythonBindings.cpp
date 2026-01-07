#include "engine/utils/pyBindings.hpp"
#include <pybind11/embed.h>
#include <pybind11/stl.h> 
#include "engine/components/Transform.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/core/GameObject.hpp"
#include <string>

namespace py = pybind11;

void BindGameObject(py::module_& m);
void BindComponent(py::module_& m);
void BindTransform(py::module_& m);
void BindInput(py::module_& m);
void BindSpriteRenderer(py::module_& m);

PYBIND11_EMBEDDED_MODULE(engine_api, m) {
    m.doc() = "C++ Core Logic for Python Handles"; 
    BindGameObject(m);
    BindComponent(m);
    BindTransform(m);
    BindSpriteRenderer(m);
    BindInput(m);


} 

namespace engine {
    void RegisterPythonBindings() {
  
    }
}