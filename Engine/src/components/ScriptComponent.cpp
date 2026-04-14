#include <pybind11/gil.h>
#include <pybind11/embed.h>
#include "engine/components/ScriptComponent.hpp"
#include <iostream>
#include <filesystem>
#include "engine/utils/EngineUtils.hpp"

using namespace EngineUtils;
namespace fs = std::filesystem;
namespace py = pybind11;

struct ScriptInstanceData {
    py::object scriptInstance;
};

ScriptComponent::ScriptComponent()
    : m_pyData(std::make_unique<ScriptInstanceData>()) {}

void ScriptComponent::CallMethod(const char* funcName)
{
    py::gil_scoped_acquire gil;
    auto& inst = m_pyData->scriptInstance;
    if (!inst || inst.is_none()) return;
    try {
        if (py::hasattr(inst, funcName)) {
            inst.attr(funcName)();
        }
    } catch (const py::error_already_set& e) {
        std::cerr << "[ScriptComponent] Python error in " << funcName << "():\n"
                  << e.what() << std::endl;
    }
}

void ScriptComponent::CallMethodStr(const char* funcName, const char* arg)
{
    py::gil_scoped_acquire gil;
    auto& inst = m_pyData->scriptInstance;
    if (!inst || inst.is_none()) return;
    try {
        if (py::hasattr(inst, funcName)) {
            inst.attr(funcName)(arg);
        }
    } catch (const py::error_already_set& e) {
        std::cerr << "[ScriptComponent] Python error in " << funcName << "():\n"
                  << e.what() << std::endl;
    }
}

YAML::Node ScriptComponent::Serialize()
{
    YAML::Node node = Component::Serialize();
    node["module"] = moduleName;
    node["class"] = className;

    // Serialize exposed field values
    auto& scriptInstance = m_pyData->scriptInstance;
    if (!m_fields.empty() && scriptInstance && !scriptInstance.is_none()) {
        py::gil_scoped_acquire gil;
        YAML::Node fieldsNode;
        for (const auto& field : m_fields) {
            try {
                py::object val = py::getattr(scriptInstance, field.name.c_str());
                if (field.typeName == "float") {
                    fieldsNode[field.name] = val.cast<float>();
                } else if (field.typeName == "int") {
                    fieldsNode[field.name] = val.cast<int>();
                } else if (field.typeName == "bool") {
                    fieldsNode[field.name] = val.cast<bool>();
                } else if (field.typeName == "str") {
                    fieldsNode[field.name] = val.cast<std::string>();
                } else if (field.typeName == "vec2") {
                    float x = py::getattr(val, "x").cast<float>();
                    float y = py::getattr(val, "y").cast<float>();
                    fieldsNode[field.name].push_back(x);
                    fieldsNode[field.name].push_back(y);
                } else if (field.typeName == "vec3") {
                    fieldsNode[field.name].push_back(py::getattr(val, "x").cast<float>());
                    fieldsNode[field.name].push_back(py::getattr(val, "y").cast<float>());
                    fieldsNode[field.name].push_back(py::getattr(val, "z").cast<float>());
                } else if (field.typeName == "vec4") {
                    fieldsNode[field.name].push_back(py::getattr(val, "x").cast<float>());
                    fieldsNode[field.name].push_back(py::getattr(val, "y").cast<float>());
                    fieldsNode[field.name].push_back(py::getattr(val, "z").cast<float>());
                    fieldsNode[field.name].push_back(py::getattr(val, "w").cast<float>());
                }
            }
            catch (const std::exception& e) {
                std::cerr << "[ScriptComponent] Error serializing field '" << field.name
                          << "': " << e.what() << std::endl;
            }
        }
        if (fieldsNode.size() > 0) {
            node["fields"] = fieldsNode;
        }
    }

    return node;
}

void ScriptComponent::Deserialize(const YAML::Node& node)
{
    Component::Deserialize(node);
    moduleName = node["module"].as<std::string>();
    className  = node["class"].as<std::string>();

    // Store pending field values to apply after instantiation
    if (node["fields"]) {
        for (auto it = node["fields"].begin(); it != node["fields"].end(); ++it) {
            m_pendingFieldValues[it->first.as<std::string>()] = it->second;
        }
    }

    state = State::Loaded;
}
 
void ScriptComponent::Init(){
    InstantiateScript();
    IntrospectFields();
    ApplyPendingFields();
    CallMethod("init");
    state = State::Initialized;
}
void ScriptComponent::Awake(){ 
    if (state >= State::Awakened) return;
    if (container->GetMode() == Container::Mode::Runtime){ 
        CallMethod("awake");
    } 
    state = State::Awakened;
}
void ScriptComponent::Start(){ 
    if (state >= State::Started) return;
    if (container->GetMode() == Container::Mode::Runtime){ 
        CallMethod("start");
    } 
    state = State::Started;
}
void ScriptComponent::Update()      { if (container->GetMode() == Container::Mode::Runtime) CallMethod("update"); }
void ScriptComponent::FixedUpdate() { if (container->GetMode() == Container::Mode::Runtime) CallMethod("fixed_update"); }
void ScriptComponent::LateUpdate()  { if (container->GetMode() == Container::Mode::Runtime) CallMethod("late_update"); }
 
void ScriptComponent::OnCollisionEnter(GameObject* other) { if (container->GetMode() == Container::Mode::Runtime) CallMethodStr("handle_collision_enter", other->GetID().c_str());
}
void ScriptComponent::OnCollisionExit(GameObject* other) { if (container->GetMode() == Container::Mode::Runtime) CallMethodStr("handle_collision_exit", other->GetID().c_str());
}
void ScriptComponent::OnTriggerEnter(GameObject* other) { if (container->GetMode() == Container::Mode::Runtime) CallMethodStr("handle_trigger_enter", other->GetID().c_str());
}
void ScriptComponent::OnTriggerExit(GameObject* other) { if (container->GetMode() == Container::Mode::Runtime) CallMethodStr("handle_trigger_exit", other->GetID().c_str());
}


void ScriptComponent::Destroy() {
    py::gil_scoped_acquire gil;
    CallMethod("on_destroy"); 
    m_pyData->scriptInstance = py::object();
}

void ScriptComponent::Shutdown() {
    if (Py_IsInitialized()) {
        py::gil_scoped_acquire gil;
        m_pyData->scriptInstance = py::object(); 
    }
}

void ScriptComponent::InstantiateScript()
{
    if (moduleName.empty() || className.empty()) {
        std::cerr << "[ScriptComponent] Module or class empty\n";
        m_pyData->scriptInstance = py::none();
        return;
    }

    py::gil_scoped_acquire gil;

    try {
        py::module sys = py::module::import("sys");
        py::list path = sys.attr("path");

        std::string scriptsPath = GetAssetPath("Domain/sandbox/scripts");
        std::string libPath = GetAssetPath("Domain/lib");

        std::string projectRoot = GetAssetPath("");

        std::vector<std::string> folders = {
            projectRoot,                            
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
        m_pyData->scriptInstance = cls();

        m_pyData->scriptInstance.attr("_component_id") = GetID();
        GameObject* go = GetGameObject();
        if (go)
            m_pyData->scriptInstance.attr("_gameobject_id") = go->GetID();
        else
            std::cerr << "[ScriptComponent] Warning: GameObject not found for script.\n";

    }
    catch (const py::error_already_set& e) {
        std::cerr << "[ScriptComponent] Python error in InstantiateScript():\n"
            << e.what() << std::endl;
        m_pyData->scriptInstance = py::none();
    }
    catch (const std::exception& e) {
        std::cerr << "[ScriptComponent] C++ exception in InstantiateScript():\n"
            << e.what() << std::endl;
        m_pyData->scriptInstance = py::none();
    }
}


void ScriptComponent::IntrospectFields()
{
    auto& scriptInstance = m_pyData->scriptInstance;
    if (!scriptInstance || scriptInstance.is_none()) return;
    
    py::gil_scoped_acquire gil;
    m_fields.clear();

    try {
        py::module introspection = py::module::import("Domain.lib.utils.introspection");
        py::object cls = scriptInstance.attr("__class__");
        py::list fieldList = introspection.attr("get_exposed_fields")(cls);

        for (auto item : fieldList) {
            py::dict d = item.cast<py::dict>();
            ScriptFieldInfo info;
            info.name = d["name"].cast<std::string>();
            info.typeName = d["type_name"].cast<std::string>();
            info.step = d["step"].cast<float>();
            info.tooltip = d["tooltip"].cast<std::string>();

            if (!d["min"].is_none())
                info.min = d["min"].cast<float>();

            if (!d["max"].is_none())
                info.max = d["max"].cast<float>();

            // If the instance doesn't have this attribute yet, apply the default
            if (!py::hasattr(scriptInstance, info.name.c_str())) {
                py::object defaultVal = d["default"];
                if (!defaultVal.is_none()) {
                    py::setattr(scriptInstance, info.name.c_str(), defaultVal);
                }
            }

            info.changeEvent = Observable::CreateEvent();
            m_fields.push_back(std::move(info));
        }
    }
    catch (const py::error_already_set& e) {
        std::cerr << "[ScriptComponent] Python error in IntrospectFields():\n"
            << e.what() << std::endl;
    }
}

void ScriptComponent::ApplyPendingFields()
{
    auto& scriptInstance = m_pyData->scriptInstance;
    if (!scriptInstance || scriptInstance.is_none()) return;
    if (m_pendingFieldValues.empty()) return;

    py::gil_scoped_acquire gil;

    for (const auto& field : m_fields) {
        auto it = m_pendingFieldValues.find(field.name);
        if (it == m_pendingFieldValues.end()) continue;

        try {
            const YAML::Node& val = it->second;
            if (field.typeName == "float") {
                py::setattr(scriptInstance, field.name.c_str(), py::float_(val.as<float>()));
            } else if (field.typeName == "int") {
                py::setattr(scriptInstance, field.name.c_str(), py::int_(val.as<int>()));
            } else if (field.typeName == "bool") {
                py::setattr(scriptInstance, field.name.c_str(), py::bool_(val.as<bool>()));
            } else if (field.typeName == "str") {
                py::setattr(scriptInstance, field.name.c_str(), py::str(val.as<std::string>()));
            } else if (field.typeName == "vec2") {
                auto seq = val.as<std::vector<float>>();
                if (seq.size() == 2) {
                    py::module re_math = py::module::import("Domain.lib.utils.re_math");
                    py::object vec2Cls = re_math.attr("Vector2");
                    py::setattr(scriptInstance, field.name.c_str(), vec2Cls(seq[0], seq[1]));
                }
            } else if (field.typeName == "vec3") {
                auto seq = val.as<std::vector<float>>();
                if (seq.size() == 3) {
                    py::module re_math = py::module::import("Domain.lib.utils.re_math");
                    py::object vec3Cls = re_math.attr("Vector3");
                    py::setattr(scriptInstance, field.name.c_str(), vec3Cls(seq[0], seq[1], seq[2]));
                }
            } else if (field.typeName == "vec4") {
                auto seq = val.as<std::vector<float>>();
                if (seq.size() == 4) {
                    py::module re_math = py::module::import("Domain.lib.utils.re_math");
                    py::object vec4Cls = re_math.attr("Vector4");
                    py::setattr(scriptInstance, field.name.c_str(), vec4Cls(seq[0], seq[1], seq[2], seq[3]));
                }
            }
        }
        catch (const std::exception& e) {
            std::cerr << "[ScriptComponent] Error applying field '" << field.name 
                      << "': " << e.what() << std::endl;
        }
    }
    m_pendingFieldValues.clear();
}

ScriptFieldValue ScriptComponent::GetFieldValue(const std::string& name)
{
    py::gil_scoped_acquire gil;
    auto& scriptInstance = m_pyData->scriptInstance;
    if (!scriptInstance || scriptInstance.is_none()) return 0.0f;

    // Find the field info to know the type
    const ScriptFieldInfo* fieldInfo = nullptr;
    for (const auto& f : m_fields) {
        if (f.name == name) { fieldInfo = &f; break; }
    }
    if (!fieldInfo) return 0.0f;

    try {
        py::object val = py::getattr(scriptInstance, name.c_str());
        if (fieldInfo->typeName == "float") {
            return val.cast<float>();
        } else if (fieldInfo->typeName == "int") {
            return val.cast<int>();
        } else if (fieldInfo->typeName == "bool") {
            return val.cast<bool>();
        } else if (fieldInfo->typeName == "str") {
            return val.cast<std::string>();
        } else if (fieldInfo->typeName == "vec2") {
            float x = py::getattr(val, "x").cast<float>();
            float y = py::getattr(val, "y").cast<float>();
            return glm::vec2(x, y);
        } else if (fieldInfo->typeName == "vec3") {
            float x = py::getattr(val, "x").cast<float>();
            float y = py::getattr(val, "y").cast<float>();
            float z = py::getattr(val, "z").cast<float>();
            return glm::vec3(x, y, z);
        } else if (fieldInfo->typeName == "vec4") {
            float x = py::getattr(val, "x").cast<float>();
            float y = py::getattr(val, "y").cast<float>();
            float z = py::getattr(val, "z").cast<float>();
            float w = py::getattr(val, "w").cast<float>();
            return glm::vec4(x, y, z, w);
        }
    }
    catch (const py::error_already_set& e) {
        std::cerr << "[ScriptComponent] Error getting field '" << name 
                  << "': " << e.what() << std::endl;
    }
    return 0.0f;
}

void ScriptComponent::SetFieldValue(const std::string& name, const ScriptFieldValue& value)
{
    py::gil_scoped_acquire gil;
    auto& scriptInstance = m_pyData->scriptInstance;
    if (!scriptInstance || scriptInstance.is_none()) return;

    try {
        std::visit([&](auto&& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, float>) {
                py::setattr(scriptInstance, name.c_str(), py::float_(v));
            } else if constexpr (std::is_same_v<T, int>) {
                py::setattr(scriptInstance, name.c_str(), py::int_(v));
            } else if constexpr (std::is_same_v<T, bool>) {
                py::setattr(scriptInstance, name.c_str(), py::bool_(v));
            } else if constexpr (std::is_same_v<T, std::string>) {
                py::setattr(scriptInstance, name.c_str(), py::str(v));
            } else if constexpr (std::is_same_v<T, glm::vec2>) {
                py::module re_math = py::module::import("Domain.lib.utils.re_math");
                py::object vec2Cls = re_math.attr("Vector2");
                py::setattr(scriptInstance, name.c_str(), vec2Cls(v.x, v.y));
            } else if constexpr (std::is_same_v<T, glm::vec3>) {
                py::module re_math = py::module::import("Domain.lib.utils.re_math");
                py::object vec3Cls = re_math.attr("Vector3");
                py::setattr(scriptInstance, name.c_str(), vec3Cls(v.x, v.y, v.z));
            } else if constexpr (std::is_same_v<T, glm::vec4>) {
                py::module re_math = py::module::import("Domain.lib.utils.re_math");
                py::object vec4Cls = re_math.attr("Vector4");
                py::setattr(scriptInstance, name.c_str(), vec4Cls(v.x, v.y, v.z, v.w));
            }
        }, value);

        // Fire the per-field change event
        for (const auto& field : m_fields) {
            if (field.name == name) {
                Notify(field.changeEvent);
                break;
            }
        }
    }
    catch (const py::error_already_set& e) {
        std::cerr << "[ScriptComponent] Error setting field '" << name 
                  << "': " << e.what() << std::endl;
    }
}


ScriptComponent::~ScriptComponent() {
    if (Py_IsInitialized() && m_pyData) {
        py::gil_scoped_acquire gil;
        m_pyData->scriptInstance = py::object();
    }
}


void ScriptComponent::Accept(IVisitor* v) {
    
    v->Visit(this); 
}

ScriptComponent* ScriptComponent::Copy(){
    ScriptComponent* copy = new ScriptComponent();
    copy->id = id;
    copy->enabled = enabled;
    copy->gameobject_id = gameobject_id;
    copy->moduleName = moduleName;
    copy->className = className;

    // Carry current field values as pending values for the copy
    auto& scriptInstance = m_pyData->scriptInstance;
    if (!m_fields.empty() && scriptInstance && !scriptInstance.is_none()) {
        py::gil_scoped_acquire gil;
        for (const auto& field : m_fields) {
            try {
                py::object val = py::getattr(scriptInstance, field.name.c_str());
                YAML::Node node;
                if (field.typeName == "float") {
                    node = val.cast<float>();
                } else if (field.typeName == "int") {
                    node = val.cast<int>();
                } else if (field.typeName == "bool") {
                    node = val.cast<bool>();
                } else if (field.typeName == "str") {
                    node = val.cast<std::string>();
                } else if (field.typeName == "vec2") {
                    node.push_back(py::getattr(val, "x").cast<float>());
                    node.push_back(py::getattr(val, "y").cast<float>());
                } else if (field.typeName == "vec3") {
                    node.push_back(py::getattr(val, "x").cast<float>());
                    node.push_back(py::getattr(val, "y").cast<float>());
                    node.push_back(py::getattr(val, "z").cast<float>());
                } else if (field.typeName == "vec4") {
                    node.push_back(py::getattr(val, "x").cast<float>());
                    node.push_back(py::getattr(val, "y").cast<float>());
                    node.push_back(py::getattr(val, "z").cast<float>());
                    node.push_back(py::getattr(val, "w").cast<float>());
                }
                copy->m_pendingFieldValues[field.name] = node;
            }
            catch (const std::exception&) {}
        }
    }

    return copy;
}

