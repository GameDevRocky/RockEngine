#include "engine/bindings/PythonBindings.hpp"
#include "engine/core/TimeManager.hpp"
#include "Engine.hpp"

void BindTime(pybind11::module_& m) {
    pybind11::module_ time_module = m.def_submodule("time_module", "Time Bindings");

    time_module.def("get_delta_time", []() -> float {
        
        return timeManager ? timeManager->DeltaTime() : 0.0f;
    });

    time_module.def("get_fixed_delta_time", []() -> float {
        
        return timeManager ? timeManager->FixedDeltaTime() : 0.0f;
    });

    time_module.def("get_elapsed_time", []() -> float {
        
        return timeManager ? timeManager->ElapsedTime() : 0.0f;
    });

    time_module.def("get_time_scale", []() -> float {
        
        return timeManager ? timeManager->TimeScale() : 1.0f;
    });

    time_module.def("set_time_scale", [](float scale) {
        
        if (timeManager) timeManager->SetTimeScale(scale);
    });

    time_module.def("get_fps", []() -> float {
        
        return timeManager ? timeManager->GetFPS() : 0.0f;
    });
}
