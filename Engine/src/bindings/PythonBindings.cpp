#include "engine/utils/pyBindings.hpp"
#include <pybind11/embed.h>
#include <pybind11/stl.h> 
#include "engine/components/Transform.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/core/GameObject.hpp"
#include <string>

namespace py = pybind11;

// SYSTEMS
void BindInputManager(py::module_& m);

// CORE
void BindGameObject(py::module_& m);


// COMPONENTS
void BindComponent(py::module_& m);
void BindTransform(py::module_& m);
void BindSpriteRenderer(py::module_& m);


PYBIND11_EMBEDDED_MODULE(rock_engine, m) {
    m.doc() = "C++ Core Logic for Python Handles"; 
    
    // CORE
    py::module_ core = m.def_submodule("core", " Core RockEngine APIs");
    BindGameObject(core);
    
    // SYSTEMS
    py::module_ systems = m.def_submodule("systems", " RockEngine systems APIs");
    BindInputManager(systems);

    // COMPONENTS
    py::module_ components = m.def_submodule("components", " RockEngine components APIs");
    BindComponent(components);
    BindTransform(components);
    BindSpriteRenderer(components);


} 

namespace engine {
    void RegisterPythonBindings() {
  
    }
}