#include <pybind11/gil.h>
#include <pybind11/embed.h>
#include "engine/components/ScriptComponent.hpp"
#include "engine/core/FileWatcherSystem.hpp"
#include <iostream>
#include <filesystem>
#include "engine/utils/EngineUtils.hpp"

using namespace EngineUtils;
namespace fs = std::filesystem;
namespace py = pybind11;

struct ScriptInstanceData {
    py::object scriptInstance;
};

namespace {
    // Ensure the project root, sandbox scripts, and lib folders are on sys.path
    // (idempotent). Returns the sandbox scripts dir. Caller must hold the GIL.
    std::string EnsureScriptPathsOnSysPath() {
        py::module sys = py::module::import("sys");
        py::list path = sys.attr("path");

        std::string scriptsPath = GetAssetPath("Domain/sandbox/scripts");
        std::vector<std::string> folders = {
            GetAssetPath(""),          // project root — for `Domain.lib...` imports
            scriptsPath,               // user scripts, imported by file stem
            GetAssetPath("Domain/lib")
        };

        for (const auto& folder : folders) {
            bool alreadyAdded = false;
            for (auto item : path) {
                if (py::str(item).cast<std::string>() == folder) {
                    alreadyAdded = true;
                    break;
                }
            }
            if (!alreadyAdded) path.append(folder);
        }
        return scriptsPath;
    }

    // Wrap a single list element ID into its Python handler object. Plain string
    // elements pass through as py::str; sprite/material refs become Sprite(id)/
    // Material(id) (empty id → None). Mirrors the scalar str-ref path.
    py::object MakeListElement(const std::string& s, const std::string& elementRefType) {
        if (elementRefType == "sprite") {
            if (s.empty()) return py::none();
            py::module_ m = py::module_::import("Domain.lib.api.rendering.sprite_handler");
            return m.attr("Sprite")(s);
        }
        if (elementRefType == "material") {
            if (s.empty()) return py::none();
            py::module_ m = py::module_::import("Domain.lib.api.rendering.material_handler");
            return m.attr("Material")(s);
        }
        return py::str(s);
    }

    // Pull a string ID out of a field value (scalar or list element): handler
    // objects expose `.id`, plain strings cast directly, None → "".
    std::string RefToIdString(const py::handle& el) {
        if (el.is_none()) return std::string();
        if (py::hasattr(el, "id")) return el.attr("id").cast<std::string>();
        return el.cast<std::string>();
    }

    // Read a Python list attribute into the matching ScriptFieldValue vector
    // alternative, dispatching on the field's element type.
    ScriptFieldValue ReadListField(const py::object& val, const ScriptFieldInfo& f) {
        const bool isList = py::isinstance<py::list>(val);
        if (f.elementTypeName == "float") {
            std::vector<float> out;
            if (isList) for (auto el : val) out.push_back(el.cast<float>());
            return out;
        }
        if (f.elementTypeName == "int") {
            std::vector<int> out;
            if (isList) for (auto el : val) out.push_back(el.cast<int>());
            return out;
        }
        if (f.elementTypeName == "bool") {
            std::vector<bool> out;
            if (isList) for (auto el : val) out.push_back(el.cast<bool>());
            return out;
        }
        // "str" and asset-ref element lists both marshal as vector<string> of IDs
        std::vector<std::string> out;
        if (isList) for (auto el : val) out.push_back(RefToIdString(el));
        return out;
    }
}

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
                    // Covers plain strings and every ref flavour (sprite/material/
                    // component:/gameobject:) — a ref holds a handler object or None,
                    // not a str, so it must go out as its id. Mirrors GetAllFieldValues.
                    fieldsNode[field.name] = RefToIdString(val);
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
                } else if (field.typeName == "list") {
                    // Variable-length sequence of the element type.
                    YAML::Node seq(YAML::NodeType::Sequence);
                    if (py::isinstance<py::list>(val)) {
                        for (auto el : val) {
                            if (field.elementTypeName == "float")
                                seq.push_back(el.cast<float>());
                            else if (field.elementTypeName == "int")
                                seq.push_back(el.cast<int>());
                            else if (field.elementTypeName == "bool")
                                seq.push_back(el.cast<bool>());
                            else
                                seq.push_back(RefToIdString(el));
                        }
                    }
                    fieldsNode[field.name] = seq;
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

    SubscribeFileWatch();

    state = State::Initialized;
}

// Subscribe to FileWatcherSystem for hot-reload of the current script file.
void ScriptComponent::SubscribeFileWatch()
{
    if (m_scriptFilePath.empty()) return;
    auto* fws = container ? container->FindSystem<FileWatcherSystem>() : nullptr;
    if (!fws) return;
    m_fileWatchSubId = fws->Subscribe([this](std::any data) {
        if (std::any_cast<std::string>(data) == m_scriptFilePath)
            ApplyHotReload();
        return true;
    }, FileWatcherSystem::FILE_CHANGED_EVENT);
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

void ScriptComponent::Shutdown() {
    if (container->GetMode() == Container::Mode::Runtime)
        CallMethod("on_shutdown");

    // Unsubscribe from FileWatcherSystem
    if (m_fileWatchSubId != -1) {
        auto* fws = container->FindSystem<FileWatcherSystem>();
        if (fws) fws->Unsubscribe(m_fileWatchSubId);
        m_fileWatchSubId = -1;
    }

    if (Py_IsInitialized() && m_pyData) {
        py::gil_scoped_acquire gil;
        m_pyData->scriptInstance = py::object();
    }
    Component::Shutdown();
}

void ScriptComponent::InstantiateScript()
{
    py::gil_scoped_acquire gil;

    if (moduleName.empty() || className.empty()) {
        std::cerr << "[ScriptComponent] Module or class empty\n";
        m_pyData->scriptInstance = py::none();
        return;
    }

    try {
        EnsureScriptPathsOnSysPath();

        py::module sys = py::module::import("sys");
        py::dict sys_modules = sys.attr("modules");
        py::module module;
        if (sys_modules.contains(moduleName.c_str())) {
            py::module importlib = py::module::import("importlib");
            module = importlib.attr("reload")(sys_modules[moduleName.c_str()]);
        } else {
            module = py::module::import(moduleName.c_str());
        }

        py::object cls = module.attr(className.c_str());
        m_pyData->scriptInstance = cls();

        m_pyData->scriptInstance.attr("_component_id") = GetID();
        GameObject* go = GetGameObject();
        if (go)
            m_pyData->scriptInstance.attr("_gameobject_id") = go->GetID();
        else
            std::cerr << "[ScriptComponent] Warning: GameObject not found for script.\n";

        // Store normalized file path for hot-reload matching
        py::module os = py::module::import("os");
        py::object os_path = os.attr("path");
        if (sys_modules.contains(moduleName.c_str())) {
            py::object mod_file = py::getattr(sys_modules[moduleName.c_str()], "__file__", py::none());
            if (!mod_file.is_none()) {
                m_scriptFilePath = os_path.attr("normcase")(
                    os_path.attr("abspath")(mod_file)).cast<std::string>();
            }
        }

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


void ScriptComponent::ApplyHotReload()
{
    bool reloadSucceeded = false;
    {
        py::gil_scoped_acquire gil;
        try {
            py::module sys = py::module::import("sys");
            py::dict sys_modules = sys.attr("modules");
            if (!sys_modules.contains(moduleName.c_str())) return;

            py::module importlib = py::module::import("importlib");
            py::module mod = importlib.attr("reload")(sys_modules[moduleName.c_str()]);
            py::object cls = mod.attr(className.c_str());
            py::object newInstance = cls();

            // Transfer field values.  If transfer_fields raises (e.g. unresolvable
            // type hint in the new script) the exception propagates to the outer
            // catch → hard stop, old instance is preserved.
            py::module scriptReload = py::module::import("Domain.lib.utils.script_reload");
            scriptReload.attr("transfer_fields")(m_pyData->scriptInstance, newInstance);

            m_pyData->scriptInstance = std::move(newInstance);
            m_pyData->scriptInstance.attr("_component_id") = GetID();
            GameObject* go = GetGameObject();
            if (go) m_pyData->scriptInstance.attr("_gameobject_id") = go->GetID();
            reloadSucceeded = true;
        } catch (const py::error_already_set& e) {
            std::cerr << "[ScriptComponent] Hard stop in ApplyHotReload() — old instance preserved:\n"
                      << e.what() << std::endl;
        }
    }

    if (!reloadSucceeded) return;

    IntrospectFields();
    Notify(SCRIPT_RELOADED_EVENT);
}

std::vector<ScriptClassInfo> ScriptComponent::GetAvailableScripts()
{
    std::vector<ScriptClassInfo> result;
    if (!Py_IsInitialized()) return result;

    py::gil_scoped_acquire gil;
    try {
        std::string scriptsPath = EnsureScriptPathsOnSysPath();
        py::module disc = py::module::import("Domain.lib.utils.script_discovery");
        py::list found = disc.attr("discover_scripts")(scriptsPath);
        for (auto item : found) {
            py::dict d = item.cast<py::dict>();
            ScriptClassInfo info;
            info.moduleName = d["module"].cast<std::string>();
            info.className  = d["class"].cast<std::string>();
            result.push_back(std::move(info));
        }
    }
    catch (const py::error_already_set& e) {
        std::cerr << "[ScriptComponent] Python error in GetAvailableScripts():\n"
                  << e.what() << std::endl;
    }
    return result;
}

void ScriptComponent::SetScript(const std::string& module, const std::string& cls)
{
    if (module == moduleName && cls == className) return;

    // Tear down the current instance's hot-reload watch.
    if (m_fileWatchSubId != -1) {
        auto* fws = container ? container->FindSystem<FileWatcherSystem>() : nullptr;
        if (fws) fws->Unsubscribe(m_fileWatchSubId);
        m_fileWatchSubId = -1;
    }

    // Drop the old Python instance and its introspected/pending state — the
    // pending values belonged to the previous script and no longer apply.
    if (Py_IsInitialized() && m_pyData) {
        py::gil_scoped_acquire gil;
        m_pyData->scriptInstance = py::object();
    }
    m_fields.clear();
    m_pendingFieldValues.clear();
    m_scriptFilePath.clear();

    moduleName = module;
    className  = cls;

    // Same sequence as Init(): instantiate → introspect → apply → init → watch.
    InstantiateScript();
    IntrospectFields();
    ApplyPendingFields();
    CallMethod("init");
    SubscribeFileWatch();

    Notify(SCRIPT_RELOADED_EVENT);
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

            if (d.contains("ref_type_name") && !d["ref_type_name"].cast<std::string>().empty())
                info.refTypeName = d["ref_type_name"].cast<std::string>();

            if (d.contains("element_type_name"))
                info.elementTypeName = d["element_type_name"].cast<std::string>();
            if (d.contains("element_ref_type_name"))
                info.elementRefTypeName = d["element_ref_type_name"].cast<std::string>();

            // If the instance doesn't have this attribute yet, apply the default
            if (!py::hasattr(scriptInstance, info.name.c_str())) {
                py::object defaultVal = d["default"];
                if (!defaultVal.is_none()) {
                    // For unassigned sprite/material/component refs, set None so
                    // the field starts empty rather than as a handler bound to an
                    // empty id (and handler setters' `if value is not None`
                    // guards behave correctly).
                    const bool isRef = info.refTypeName == "sprite"
                                    || info.refTypeName == "material"
                                    || info.refTypeName.rfind("component:", 0) == 0;
                    if (isRef
                        && py::isinstance<py::str>(defaultVal)
                        && defaultVal.cast<std::string>().empty()) {
                        py::setattr(scriptInstance, info.name.c_str(), py::none());
                    } else {
                        py::setattr(scriptInstance, info.name.c_str(), defaultVal);
                    }
                }
            } else {
                // The instance already holds a value (e.g. after hot-reload
                // transferred old data). If the runtime value's type doesn't
                // match the annotated type, the value wins — re-derive the
                // displayed typeName from the live object so the inspector
                // shows the correct widget instead of throwing cast errors.
                py::object cur = py::getattr(scriptInstance, info.name.c_str());
                std::string runtimeType;
                if (py::isinstance<py::bool_>(cur)) {
                    runtimeType = "bool";
                } else if (py::isinstance<py::int_>(cur)) {
                    runtimeType = "int";
                } else if (py::isinstance<py::float_>(cur)) {
                    runtimeType = "float";
                } else if (py::isinstance<py::str>(cur)) {
                    runtimeType = "str";
                } else if (!cur.is_none()) {
                    std::string qual;
                    try { qual = cur.attr("__class__").attr("__qualname__").cast<std::string>(); }
                    catch (...) {}
                    if (qual.find("Vector4") != std::string::npos)      runtimeType = "vec4";
                    else if (qual.find("Vector3") != std::string::npos) runtimeType = "vec3";
                    else if (qual.find("Vector2") != std::string::npos) runtimeType = "vec2";
                }
                if (!runtimeType.empty() && runtimeType != info.typeName) {
                    info.typeName = runtimeType;
                    // refTypeName only applies to the annotated Sprite/Material
                    // path; if the runtime type no longer matches, clear it.
                    info.refTypeName.clear();
                }
            }

            // Give each instance its own copy of a list field so multiple
            // GameObjects sharing the same script class don't alias the
            // class-level mutable default list.
            if (info.typeName == "list" && py::hasattr(scriptInstance, info.name.c_str())) {
                py::object cur = py::getattr(scriptInstance, info.name.c_str());
                if (py::isinstance<py::list>(cur))
                    py::setattr(scriptInstance, info.name.c_str(), py::list(cur));
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
                std::string strVal = val.as<std::string>();
                if (field.refTypeName == "sprite") {
                    if (strVal.empty()) {
                        py::setattr(scriptInstance, field.name.c_str(), py::none());
                    } else {
                        py::module_ sprite_mod = py::module_::import("Domain.lib.api.rendering.sprite_handler");
                        py::setattr(scriptInstance, field.name.c_str(), sprite_mod.attr("Sprite")(strVal));
                    }
                } else if (field.refTypeName == "material") {
                    if (strVal.empty()) {
                        py::setattr(scriptInstance, field.name.c_str(), py::none());
                    } else {
                        py::module_ mat_mod = py::module_::import("Domain.lib.api.rendering.material_handler");
                        py::setattr(scriptInstance, field.name.c_str(), mat_mod.attr("Material")(strVal));
                    }
                } else if (field.refTypeName.rfind("component:", 0) == 0) {
                    // Component ref: strVal is the owning GameObject's id; wrap it
                    // in the matching component handler (empty → None).
                    std::string typeName = field.refTypeName.substr(std::string("component:").size());
                    py::module_ intro = py::module_::import("Domain.lib.utils.introspection");
                    py::setattr(scriptInstance, field.name.c_str(),
                                intro.attr("make_component_ref")(strVal, typeName));
                } else {
                    py::setattr(scriptInstance, field.name.c_str(), py::str(strVal));
                }
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
            } else if (field.typeName == "list") {
                py::list lst;
                if (val.IsSequence()) {
                    for (const auto& elNode : val) {
                        if (field.elementTypeName == "float")
                            lst.append(py::float_(elNode.as<float>()));
                        else if (field.elementTypeName == "int")
                            lst.append(py::int_(elNode.as<int>()));
                        else if (field.elementTypeName == "bool")
                            lst.append(py::bool_(elNode.as<bool>()));
                        else
                            lst.append(MakeListElement(elNode.as<std::string>(), field.elementRefTypeName));
                    }
                }
                py::setattr(scriptInstance, field.name.c_str(), lst);
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
            // Ref fields (Sprite, Material, etc.) may be None when unassigned
            if (val.is_none()) return std::string();
            // Ref fields store their ID in a .id attribute
            if (py::hasattr(val, "id")) {
                return val.attr("id").cast<std::string>();
            }
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
        } else if (fieldInfo->typeName == "list") {
            return ReadListField(val, *fieldInfo);
        }
    }
    catch (const py::error_already_set& e) {
        std::cerr << "[ScriptComponent] Error getting field '" << name
                  << "': " << e.what() << std::endl;
    }
    return 0.0f;
}

std::map<std::string, ScriptFieldValue> ScriptComponent::GetAllFieldValues()
{
    std::map<std::string, ScriptFieldValue> result;
    py::gil_scoped_acquire gil;
    auto& scriptInstance = m_pyData->scriptInstance;
    if (!scriptInstance || scriptInstance.is_none()) return result;

    for (const auto& field : m_fields) {
        try {
            py::object val = py::getattr(scriptInstance, field.name.c_str());
            if (field.typeName == "float") {
                result[field.name] = val.cast<float>();
            } else if (field.typeName == "int") {
                result[field.name] = val.cast<int>();
            } else if (field.typeName == "bool") {
                result[field.name] = val.cast<bool>();
            } else if (field.typeName == "str") {
                // Ref fields (Sprite, Material, etc.) may be None when unassigned
                if (val.is_none()) {
                    result[field.name] = std::string();
                } else if (py::hasattr(val, "id")) {
                    result[field.name] = val.attr("id").cast<std::string>();
                } else {
                    result[field.name] = val.cast<std::string>();
                }
            } else if (field.typeName == "vec2") {
                float x = py::getattr(val, "x").cast<float>();
                float y = py::getattr(val, "y").cast<float>();
                result[field.name] = glm::vec2(x, y);
            } else if (field.typeName == "vec3") {
                float x = py::getattr(val, "x").cast<float>();
                float y = py::getattr(val, "y").cast<float>();
                float z = py::getattr(val, "z").cast<float>();
                result[field.name] = glm::vec3(x, y, z);
            } else if (field.typeName == "vec4") {
                float x = py::getattr(val, "x").cast<float>();
                float y = py::getattr(val, "y").cast<float>();
                float z = py::getattr(val, "z").cast<float>();
                float w = py::getattr(val, "w").cast<float>();
                result[field.name] = glm::vec4(x, y, z, w);
            } else if (field.typeName == "list") {
                result[field.name] = ReadListField(val, field);
            }
        }
        catch (const py::error_already_set& e) {
            std::cerr << "[ScriptComponent] Error in GetAllFieldValues for '" << field.name
                      << "': " << e.what() << std::endl;
        }
    }
    return result;
}

void ScriptComponent::SetFieldValue(const std::string& name, const ScriptFieldValue& value)
{
    py::gil_scoped_acquire gil;
    auto& scriptInstance = m_pyData->scriptInstance;
    if (!scriptInstance || scriptInstance.is_none()) return;

    // Look up ref type so we can wrap Sprite/Material IDs properly
    std::string refTypeName;
    std::string elementRefType;
    for (const auto& f : m_fields) {
        if (f.name == name) { refTypeName = f.refTypeName; elementRefType = f.elementRefTypeName; break; }
    }

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
                if (refTypeName == "sprite") {
                    if (v.empty()) {
                        py::setattr(scriptInstance, name.c_str(), py::none());
                    } else {
                        py::module_ sprite_mod = py::module_::import("Domain.lib.api.rendering.sprite_handler");
                        py::setattr(scriptInstance, name.c_str(), sprite_mod.attr("Sprite")(v));
                    }
                } else if (refTypeName == "material") {
                    if (v.empty()) {
                        py::setattr(scriptInstance, name.c_str(), py::none());
                    } else {
                        py::module_ mat_mod = py::module_::import("Domain.lib.api.rendering.material_handler");
                        py::setattr(scriptInstance, name.c_str(), mat_mod.attr("Material")(v));
                    }
                } else if (refTypeName.rfind("component:", 0) == 0) {
                    // Component ref: `v` is the owning GameObject's id; wrap it in
                    // the matching component handler (empty → None). See
                    // introspection.make_component_ref.
                    std::string typeName = refTypeName.substr(std::string("component:").size());
                    py::module_ intro = py::module_::import("Domain.lib.utils.introspection");
                    py::setattr(scriptInstance, name.c_str(),
                                intro.attr("make_component_ref")(v, typeName));
                } else {
                    py::setattr(scriptInstance, name.c_str(), py::str(v));
                }
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
            } else if constexpr (std::is_same_v<T, std::vector<int>>) {
                py::list lst;
                for (int x : v) lst.append(py::int_(x));
                py::setattr(scriptInstance, name.c_str(), lst);
            } else if constexpr (std::is_same_v<T, std::vector<float>>) {
                py::list lst;
                for (float x : v) lst.append(py::float_(x));
                py::setattr(scriptInstance, name.c_str(), lst);
            } else if constexpr (std::is_same_v<T, std::vector<bool>>) {
                py::list lst;
                for (bool x : v) lst.append(py::bool_(x));
                py::setattr(scriptInstance, name.c_str(), lst);
            } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
                py::list lst;
                for (const auto& s : v) lst.append(MakeListElement(s, elementRefType));
                py::setattr(scriptInstance, name.c_str(), lst);
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

    // Carry current field values as pending values for the copy. Serialize()
    // already marshals every field type (including lists and refs) into exactly
    // the shape Deserialize() feeds m_pendingFieldValues, so reuse it rather
    // than maintaining a second copy of that marshalling here.
    YAML::Node serialized = Serialize();
    if (serialized["fields"]) {
        for (auto it = serialized["fields"].begin(); it != serialized["fields"].end(); ++it)
            copy->m_pendingFieldValues[it->first.as<std::string>()] = it->second;
    }

    return copy;
}

