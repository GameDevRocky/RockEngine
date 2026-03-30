#include <pybind11/gil.h>
#include <pybind11/embed.h>
#include "engine/components/ScriptComponent.hpp"
#include <iostream>
#include <filesystem>
#include "engine/utils/EngineUtils.hpp"

using namespace EngineUtils;
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
    state = State::Loaded;
}
 
void ScriptComponent::Init(){
    InstantiateScript();
    state = State::Initialized;
}
void ScriptComponent::Awake(){ 
    if (state >= State::Awakened) return;
    if (container->GetMode() == Container::Mode::Runtime){ 
        CallIfExists("awake");
    } 
    state = State::Awakened;
}
void ScriptComponent::Start(){ 
    if (state >= State::Started) return;
    if (container->GetMode() == Container::Mode::Runtime){ 
        CallIfExists("start");
    } 
    state = State::Started;
}
void ScriptComponent::Update()      { if (container->GetMode() == Container::Mode::Runtime) CallIfExists("update"); }
void ScriptComponent::FixedUpdate() { if (container->GetMode() == Container::Mode::Runtime) CallIfExists("fixed_update"); }
void ScriptComponent::LateUpdate()  { if (container->GetMode() == Container::Mode::Runtime) CallIfExists("late_update"); }

void ScriptComponent::Destroy() {
    py::gil_scoped_acquire gil; // Lock here first
    CallIfExists("on_destroy"); 
    scriptInstance = py::object();
}

void ScriptComponent::Shutdown() {
    if (Py_IsInitialized()) {
        py::gil_scoped_acquire gil;
        // Releasing the handle to the interpreter and nullifying the C++ wrapper
        scriptInstance = py::object(); 
    }
}

void ScriptComponent::InstantiateScript()
{
    if (moduleName.empty() || className.empty()) {
        std::cerr << "[ScriptComponent] Module or class empty\n";
        scriptInstance = py::none();
        return;
    }

    py::gil_scoped_acquire gil;

    try {
        py::module sys = py::module::import("sys");
        py::list path = sys.attr("path");

        std::string scriptsPath = GetAssetPath("Domain/sandbox/scripts");
        std::string libPath = GetAssetPath("Domain/lib");

        std::string projectRoot = GetAssetPath(""); // This returns the C:/.../RockEngine path

        // 2. Add it to the list of folders Python checks
        std::vector<std::string> folders = {
            projectRoot,                             // Add this so 'import Domain.x' works
            scriptsPath,
            libPath
        };

        for (const auto& folder : folders) {
            bool alreadyAdded = false;
            for (auto item : path) {
                if (py::str(item).cast<std::string>() == folder) {
                    alreadyAdded = true;
                    break;
                }
            }
            if (!alreadyAdded) {
                path.append(folder);
            }
        }

        py::module module = py::module::import(moduleName.c_str());

        py::object cls = module.attr(className.c_str());
        scriptInstance = cls();

        scriptInstance.attr("_component_id") = GetID();
        GameObject* go = GetGameObject();
        if (go)
            scriptInstance.attr("_gameobject_id") = go->GetID();
        else
            std::cerr << "[ScriptComponent] Warning: GameObject not found for script.\n";

    }
    catch (const py::error_already_set& e) {
        std::cerr << "[ScriptComponent] Python error in InstantiateScript():\n"
            << e.what() << std::endl;
        scriptInstance = py::none();
    }
    catch (const std::exception& e) {
        std::cerr << "[ScriptComponent] C++ exception in InstantiateScript():\n"
            << e.what() << std::endl;
        scriptInstance = py::none();
    }
}

void ScriptComponent::CallIfExists(const char* funcName)
{
    py::gil_scoped_acquire gil;
    if (!scriptInstance || scriptInstance.is_none()) {
        std::cerr << "[ScriptComponent] scriptInstance invalid in " << funcName << std::endl;
        return;
    }
    if (!Py_IsInitialized()) {
        std::cerr << "[ScriptComponent] Python is NOT initialized in "
                << funcName << std::endl;
        return;
    }

    

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
ScriptComponent::~ScriptComponent() {
    if (Py_IsInitialized()) {
        py::gil_scoped_acquire gil;
        scriptInstance = py::none();
    }
    // If Python is finalized, just leak the ref — Python already cleaned it up
}

ScriptComponent* ScriptComponent::Copy(){
    ScriptComponent* copy = new ScriptComponent();
    copy->id = id;
    copy->enabled = enabled;
    copy->gameobject_id = gameobject_id;
    copy->moduleName = moduleName;
    copy->className = className;
    return copy;
}

