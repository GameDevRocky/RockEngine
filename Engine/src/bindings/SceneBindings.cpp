#include "engine/bindings/PythonBindings.hpp"
#include "engine/core/SceneManager.hpp"

void BindScene(pybind11::module_& m) {
    pybind11::module_ scene_module = m.def_submodule("scene_module", "SceneManager Bindings");

    // allow_runtime lets a script explicitly persist a Play-mode (Runtime
    // container) scene back to its file on disk — see the header comment on
    // SceneManager::SaveScene for why this is opt-in rather than the default.
    scene_module.def("save_scene", [](const std::string& scene_id, bool allow_runtime) {
        if (!sceneManager) return false;
        return sceneManager->SaveScene(scene_id, allow_runtime);
    });
}
