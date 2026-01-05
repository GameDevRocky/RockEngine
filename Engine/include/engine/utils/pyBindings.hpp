#pragma once

namespace engine {
    // This function can be used to manually trigger registrations 
    // or checks, though PYBIND11_EMBEDDED_MODULE runs automatically.
    void RegisterPythonBindings();
}