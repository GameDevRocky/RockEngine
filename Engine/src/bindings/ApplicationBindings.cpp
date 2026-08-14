#include "engine/bindings/PythonBindings.hpp"
#include "Engine.hpp"

// Mirrors Unity's Application.isEditor. Game code needs to be able to ask "am I running
// inside the tools or in a shipped build?" so debug cheats, dev overlays and profiling
// hooks can compile out of the thing a player actually receives.
//
// Note this is NOT the same question as "am I in play mode" -- a script asking that wants
// Container::Mode, which it already gets implicitly by virtue of only running in Runtime.
void BindApplication(pybind11::module_& m) {
    pybind11::module_ application_module = m.def_submodule("application_module", "Application Bindings");

    application_module.def("is_editor", []() { return Engine::Get()->IsEditor(); });
    application_module.def("is_player", []() { return Engine::Get()->IsPlayer(); });
}
