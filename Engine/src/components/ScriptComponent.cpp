#include "engine/components/ScriptComponent.hpp"
#include <pybind11/embed.h>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

YAML::Node ScriptComponent::Serialize()
{
    YAML::Node node = Component::Serialize();
    node["module"] = moduleName;
    node["class"] = className;
    return node;
}

void ScriptComponent::Deserialize(const YAML::Node& node)
{
    Component::Deserialize(node);
    moduleName = node["module"].as<std::string>();
    className  = node["class"].as<std::string>();
}

void ScriptComponent::Awake()
{
    InstantiateScript();
    CallIfExists("awake");
}

void ScriptComponent::Start()       { CallIfExists("start"); }
void ScriptComponent::Update()      { CallIfExists("update"); }
void ScriptComponent::FixedUpdate() { CallIfExists("fixed_update"); }
void ScriptComponent::LateUpdate()  { CallIfExists("late_update"); }

void ScriptComponent::OnDestroy()
{
    CallIfExists("on_destroy");
    scriptInstance = py::none();
}

void ScriptComponent::InstantiateScript()
{
    if (moduleName.empty() || className.empty())
        return;

    py::gil_scoped_acquire gil;

    try {
        py::module sys = py::module::import("sys");
        py::list path = sys.attr("path");
        fs::path scriptsFolder = fs::absolute("Domain/assets/scripts");
        fs::path libFolder     = fs::absolute("Domain/lib");
        std::vector<std::string> folders = { scriptsFolder.string(), libFolder.string() };
        for (auto& folder : folders) {
            bool alreadyAdded = false;
            for (auto item : path) {
                if (std::string(py::str(item)) == folder) {
                    alreadyAdded = true;
                    break;
                }
            }
            if (!alreadyAdded)
                path.append(folder);
        }

        py::module module = py::module::import(moduleName.c_str());
        py::object cls = module.attr(className.c_str());
        scriptInstance = cls();
        GameObject* go = GetGameObject();
        if (go)
            scriptInstance.attr("_gameobject_id") = go->GetID();
        else
            std::cerr << "[ScriptComponent] Warning: GameObject not found for script.\n";

    } catch (const py::error_already_set& e) {
        std::cerr << "[ScriptComponent] Python error in InstantiateScript():\n"
                  << e.what() << std::endl;
        scriptInstance = py::none();
    } catch (const std::exception& e) {
        std::cerr << "[ScriptComponent] C++ exception in InstantiateScript():\n"
                  << e.what() << std::endl;
        scriptInstance = py::none();
    }
}


void ScriptComponent::CallIfExists(const char* funcName)
{
    if (!scriptInstance || scriptInstance.is_none()) {
        std::cerr << "[ScriptComponent] scriptInstance invalid in " << funcName << std::endl;
        return;
    }
    if (!Py_IsInitialized()) {
        std::cerr << "[ScriptComponent] Python is NOT initialized in "
                << funcName << std::endl;
        return;
    }

    py::gil_scoped_acquire gil;

    if (!py::hasattr(scriptInstance, funcName))
        return;

    try {
        scriptInstance.attr(funcName)();
    } catch (const py::error_already_set& e) {
        std::cerr << "[ScriptComponent] Python error in "
                  << funcName << ":\n"
                  << e.what() << std::endl;
    }
}
