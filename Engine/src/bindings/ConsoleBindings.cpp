#include "engine/bindings/PythonBindings.hpp"
#include "engine/core/InputManager.hpp"
#include <iostream>
#include "Engine.hpp"
#include "engine/debug/Console.hpp"


void BindConsoleManager(pybind11::module_& m) {
    pybind11::module_ console_module = m.def_submodule("console_module", "Console Log Bindings");
    
    console_module.def("comment", [](std::string& message) {
        Console::Comment(message);
    });
    
    console_module.def("warn", [](std::string& message) {
        Console::Comment(message);
    }); 

    console_module.def("alert", [](std::string& message) {
        Console::Comment(message);
    });
}