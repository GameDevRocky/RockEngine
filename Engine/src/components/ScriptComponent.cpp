#include <pybind11/gil.h>
#include <pybind11/embed.h>
#include "engine/components/ScriptComponent.hpp"
#include "engine/core/FileWatcherSystem.hpp"
#include "engine/debug/FrameProfiler.hpp"
#include <iostream>
#include <filesystem>
#include "engine/utils/EngineUtils.hpp"

using namespace EngineUtils;
namespace fs = std::filesystem;
namespace py = pybind11;

struct ScriptInstanceData {
    py::object scriptInstance;
    // Bound per-frame lifecycle methods, resolved once per instance (and again
    // on hot-reload) so the frame loop skips the GIL + two string MRO lookups
    // that py::hasattr + attr() cost per call. Cleared whenever scriptInstance
    // is reset; must be released under the GIL like scriptInstance itself.
    py::object updateFn;
    py::object fixedUpdateFn;
    py::object lateUpdateFn;
    bool hasUpdate = false;
    bool hasFixedUpdate = false;
    bool hasLateUpdate = false;
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

    // True if `cur`'s runtime type is usable as a field of declared type
    // `typeName`. Used after a hot-reload to decide whether a value carried over
    // from the previous instance survives, or is reset to the new default when
    // the field's type annotation changed (e.g. list[float] -> int). Keeps the
    // stored value castable to the declared type so the inspector never casts a
    // list to an int (which threw an uncaught pybind cast_error and crashed).
    bool ValueMatchesFieldType(const py::object& cur, const std::string& typeName,
                               const std::string& elementTypeName = "") {
        if (typeName == "bool")  return py::isinstance<py::bool_>(cur);
        // bool subclasses int in Python, so exclude it from int/float.
        if (typeName == "int")   return py::isinstance<py::int_>(cur) && !py::isinstance<py::bool_>(cur);
        if (typeName == "float") return !py::isinstance<py::bool_>(cur) &&
                                         (py::isinstance<py::float_>(cur) || py::isinstance<py::int_>(cur));
        // "str" covers plain strings and ref fields, whose value is None (unassigned)
        // or a handler object carrying an .id — all valid, so none are reset.
        if (typeName == "str")   return cur.is_none() || py::isinstance<py::str>(cur) || py::hasattr(cur, "id");
        if (typeName == "list") {
            if (!py::isinstance<py::list>(cur)) return false;
            // A list carried across a hot-reload keeps its old ELEMENTS, and being
            // a list is not enough to make them readable: vec elements are read by
            // attribute (.x/.y/.z), so a list[float] re-annotated as
            // list[Vector3] would throw on every read and show up as an empty
            // list. One element decides it — a list of mixed types cannot be
            // authored from the inspector or expressed by the annotation.
            py::list elements = py::reinterpret_borrow<py::list>(cur);
            if (elements.empty() || elementTypeName.empty()) return true;
            return ValueMatchesFieldType(elements[0].cast<py::object>(), elementTypeName);
        }
        if (typeName == "vec2" || typeName == "vec3" || typeName == "vec4") {
            if (cur.is_none()) return false;
            std::string qual;
            try { qual = cur.attr("__class__").attr("__qualname__").cast<std::string>(); } catch (...) {}
            const char* want = typeName == "vec4" ? "Vector4" : typeName == "vec3" ? "Vector3" : "Vector2";
            return qual.find(want) != std::string::npos;
        }
        return true; // unknown type — don't second-guess it
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
        if (elementRefType.rfind("gameobject:", 0) == 0) {
            py::module_ intro = py::module_::import("Domain.lib.utils.introspection");
            return intro.attr("make_gameobject_ref")(s);   // empty id → None
        }
        if (elementRefType.rfind("component:", 0) == 0) {
            std::string typeName = elementRefType.substr(std::string("component:").size());
            py::module_ intro = py::module_::import("Domain.lib.utils.introspection");
            return intro.attr("make_component_ref")(s, typeName);   // empty id → None
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

    // Normalize a path exactly the way file_watcher.py normalizes the paths it
    // reports (os.path.normcase(os.path.abspath(p))), so the two can be compared
    // as plain strings. Going through Python rather than std::filesystem is the
    // point: matching its casing/separator rules by hand is what would drift.
    // Caller must hold the GIL.
    std::string NormalizeWatchPath(const std::string& path) {
        if (path.empty()) return path;
        try {
            py::object os_path = py::module::import("os").attr("path");
            return os_path.attr("normcase")(os_path.attr("abspath")(path)).cast<std::string>();
        } catch (const py::error_already_set&) {
            return path;
        }
    }

    // Component count of a "vec2"/"vec3"/"vec4" type name, 0 for anything else.
    // Lets the list paths below treat the three vector widths as one case.
    int VecWidth(const std::string& typeName) {
        if (typeName == "vec2") return 2;
        if (typeName == "vec3") return 3;
        if (typeName == "vec4") return 4;
        return 0;
    }

    // Read the first `n` components of a Python Vector2/3/4 handler. Widened to a
    // vec4 so one signature covers all three; the caller truncates.
    glm::vec4 ReadVecObject(const py::handle& val, int n) {
        static const char* kComponents[4] = { "x", "y", "z", "w" };
        glm::vec4 out(0.0f);
        for (int i = 0; i < n; ++i)
            out[i] = py::getattr(val, kComponents[i]).cast<float>();
        return out;
    }

    // The inverse: build the Python Vector`n` from a widened vec4.
    py::object MakeVecObject(const glm::vec4& v, int n) {
        py::module re_math = py::module::import("Domain.lib.utils.re_math");
        if (n <= 2) return re_math.attr("Vector2")(v.x, v.y);
        if (n == 3) return re_math.attr("Vector3")(v.x, v.y, v.z);
        return re_math.attr("Vector4")(v.x, v.y, v.z, v.w);
    }

    // Read a Python list attribute into the matching ScriptFieldValue vector
    // alternative, dispatching on the field's element type.
    ScriptFieldValue ReadListField(const py::object& val, const ScriptFieldInfo& f) {
        const bool isList = py::isinstance<py::list>(val);
        if (f.elementTypeName == "vec2") {
            std::vector<glm::vec2> out;
            if (isList) for (auto el : val) out.push_back(glm::vec2(ReadVecObject(el, 2)));
            return out;
        }
        if (f.elementTypeName == "vec3") {
            std::vector<glm::vec3> out;
            if (isList) for (auto el : val) out.push_back(glm::vec3(ReadVecObject(el, 3)));
            return out;
        }
        if (f.elementTypeName == "vec4") {
            std::vector<glm::vec4> out;
            if (isList) for (auto el : val) out.push_back(ReadVecObject(el, 4));
            return out;
        }
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
    ROCK_PROFILE_SCOPE("Scripts");
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
                    const int elemWidth = VecWidth(field.elementTypeName);
                    YAML::Node seq(YAML::NodeType::Sequence);
                    if (py::isinstance<py::list>(val)) {
                        for (auto el : val) {
                            if (elemWidth > 0) {
                                // Vector elements nest: the scalar vecN branches
                                // above write a flat sequence of components, so a
                                // list of them is a sequence of those.
                                const glm::vec4 c = ReadVecObject(el, elemWidth);
                                YAML::Node comps(YAML::NodeType::Sequence);
                                for (int i = 0; i < elemWidth; ++i) comps.push_back(c[i]);
                                seq.push_back(comps);
                            }
                            else if (field.elementTypeName == "float")
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
    else if (!m_pendingFieldValues.empty()) {
        // No live instance to read from — either the script is missing, or this
        // component was deserialized and never initialized. Either way the pending
        // values are the last thing known about these fields, and they are already
        // in exactly the shape Deserialize() expects. Writing them back out is what
        // keeps a missing script's authored values from being erased by the next
        // scene save; without this, deleting a .py file and saving would silently
        // discard every value tuned on it.
        YAML::Node fieldsNode;
        for (const auto& [name, value] : m_pendingFieldValues)
            fieldsNode[name] = value;
        node["fields"] = fieldsNode;
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
    RebuildInstance();
    SubscribeFileWatch();

    state = State::Initialized;
}

// Subscribe to FileWatcherSystem for hot-reload of the current script file, and
// for its disappearance. InstantiateScript leaves m_scriptFilePath pointing at
// where the module is expected to live even when the import failed, so a missing
// script is watched too — that is what lets restoring the file heal it live.
void ScriptComponent::SubscribeFileWatch()
{
    if (m_scriptFilePath.empty()) return;
    auto* fws = container ? container->FindSystem<FileWatcherSystem>() : nullptr;
    if (!fws) return;
    m_fileWatchSubId = fws->Subscribe([this](std::any data) {
        if (std::any_cast<std::string>(data) != m_scriptFilePath) return true;
        // A missing script has no instance to transfer state from, and its module
        // may never have imported at all, so ApplyHotReload has nothing to work
        // with — rebuild from scratch instead.
        if (m_missing) {
            RebuildInstance();
            // Unconditional: a successful rebuild clears the banner, and a failed
            // one keeps it — either way the inspector must re-read the state.
            Notify(SCRIPT_RELOADED_EVENT);
        } else {
            ApplyHotReload();
        }
        return true;
    }, FileWatcherSystem::FILE_CHANGED_EVENT);

    m_fileDeleteSubId = fws->Subscribe([this](std::any data) {
        if (std::any_cast<std::string>(data) == m_scriptFilePath)
            MarkMissing();
        return true;
    }, FileWatcherSystem::FILE_DELETED_EVENT);
}

void ScriptComponent::UnsubscribeFileWatch()
{
    auto* fws = container ? container->FindSystem<FileWatcherSystem>() : nullptr;
    if (fws) {
        if (m_fileWatchSubId  != -1) fws->Unsubscribe(m_fileWatchSubId);
        if (m_fileDeleteSubId != -1) fws->Unsubscribe(m_fileDeleteSubId);
    }
    m_fileWatchSubId  = -1;
    m_fileDeleteSubId = -1;
}

void ScriptComponent::RebuildInstance()
{
    InstantiateScript();
    IntrospectFields();
    ApplyPendingFields();
    CallMethod("init");
}

void ScriptComponent::MarkMissing()
{
    if (m_missing) return;

    // Trust the filesystem over the event. A rename reports a deletion of the old
    // path, and several editors save by writing a temp file and renaming it over
    // the target — both of which can name a path that exists again by the time this
    // runs, one frame later. Only a path that is really gone is a missing script.
    if (!m_scriptFilePath.empty() && std::filesystem::exists(m_scriptFilePath)) return;

    // Preserve whatever the user tuned before the instance goes away. These are
    // re-applied verbatim by ApplyPendingFields when the script comes back, and
    // written straight back out by Serialize() in the meantime.
    CaptureFieldsAsPending();

    if (Py_IsInitialized() && m_pyData) {
        py::gil_scoped_acquire gil;
        // Clearing the cached bound methods matters as much as clearing the
        // instance: updateFn and friends hold their own strong reference, so a
        // deleted script would otherwise keep ticking every frame.
        m_pyData->scriptInstance = py::object();
        RefreshMethodCache();
    }
    m_fields.clear();
    m_lastPolledValues.clear();
    m_missing = true;

    std::cerr << "[ScriptComponent] Script '" << className << "' is missing — "
              << m_scriptFilePath << " no longer exists.\n";

    Notify(SCRIPT_RELOADED_EVENT);
}

void ScriptComponent::CaptureFieldsAsPending()
{
    if (m_fields.empty()) return;
    auto& inst = m_pyData->scriptInstance;
    if (!inst || inst.is_none()) return;

    // Serialize() already marshals every field type (lists and refs included) into
    // the shape m_pendingFieldValues holds, so reuse it rather than maintaining a
    // second copy of that marshalling here — the same reasoning as Copy().
    YAML::Node serialized = Serialize();
    if (!serialized["fields"]) return;
    for (auto it = serialized["fields"].begin(); it != serialized["fields"].end(); ++it)
        m_pendingFieldValues[it->first.as<std::string>()] = it->second;
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
namespace {
    // Fast path for the cached lifecycle methods: the has-flag is checked
    // before taking the GIL, so scripts without a given phase cost nothing.
    void CallBound(const py::object& fn, const char* name)
    {
        ROCK_PROFILE_SCOPE("Scripts");
        py::gil_scoped_acquire gil;
        try {
            fn();
        } catch (const py::error_already_set& e) {
            std::cerr << "[ScriptComponent] Python error in " << name << "():\n"
                      << e.what() << std::endl;
        }
    }
}

void ScriptComponent::Update()      { if (container->GetMode() == Container::Mode::Runtime && m_pyData->hasUpdate)      CallBound(m_pyData->updateFn, "update"); }
void ScriptComponent::FixedUpdate() { if (container->GetMode() == Container::Mode::Runtime && m_pyData->hasFixedUpdate) CallBound(m_pyData->fixedUpdateFn, "fixed_update"); }
void ScriptComponent::LateUpdate()  { if (container->GetMode() == Container::Mode::Runtime && m_pyData->hasLateUpdate)  CallBound(m_pyData->lateUpdateFn, "late_update"); }
 
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

    UnsubscribeFileWatch();

    if (Py_IsInitialized() && m_pyData) {
        py::gil_scoped_acquire gil;
        m_pyData->scriptInstance = py::object();
        RefreshMethodCache();
    }
    Component::Shutdown();
}

void ScriptComponent::RefreshMethodCache()
{
    auto& d = *m_pyData;
    d.updateFn = py::object();
    d.fixedUpdateFn = py::object();
    d.lateUpdateFn = py::object();
    d.hasUpdate = d.hasFixedUpdate = d.hasLateUpdate = false;

    auto& inst = d.scriptInstance;
    if (!inst || inst.is_none()) return;
    if (py::hasattr(inst, "update"))       { d.updateFn      = inst.attr("update");       d.hasUpdate      = true; }
    if (py::hasattr(inst, "fixed_update")) { d.fixedUpdateFn = inst.attr("fixed_update"); d.hasFixedUpdate = true; }
    if (py::hasattr(inst, "late_update"))  { d.lateUpdateFn  = inst.attr("late_update");  d.hasLateUpdate  = true; }
}

void ScriptComponent::InstantiateScript()
{
    py::gil_scoped_acquire gil;

    if (moduleName.empty() || className.empty()) {
        // Unassigned, not broken — the inspector's script picker covers this case,
        // so it is deliberately not the missing-script error state.
        m_pyData->scriptInstance = py::none();
        RefreshMethodCache();
        m_missing = false;
        m_scriptFilePath.clear();
        return;
    }

    // Where the module is expected to live, established up front so a failed
    // import still has a path to watch for the file coming back. A successful
    // import overwrites it with the module's real __file__ below.
    m_scriptFilePath = NormalizeWatchPath(
        GetAssetPath("Domain/sandbox/scripts/" + moduleName + ".py"));

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
        if (sys_modules.contains(moduleName.c_str())) {
            py::object mod_file = py::getattr(sys_modules[moduleName.c_str()], "__file__", py::none());
            if (!mod_file.is_none())
                m_scriptFilePath = NormalizeWatchPath(mod_file.cast<std::string>());
        }

        m_missing = false;
    }
    catch (const py::error_already_set& e) {
        // The class could not be resolved: a deleted/renamed .py file, a class
        // removed from it, or a script that fails at import. All of them leave the
        // component pointing at something that isn't there, which is the missing
        // state — module/class and pending field values are kept so restoring the
        // file restores the component.
        std::cerr << "[ScriptComponent] Python error in InstantiateScript():\n"
            << e.what() << std::endl;
        m_pyData->scriptInstance = py::none();
        m_missing = true;
    }
    catch (const std::exception& e) {
        std::cerr << "[ScriptComponent] C++ exception in InstantiateScript():\n"
            << e.what() << std::endl;
        m_pyData->scriptInstance = py::none();
        m_missing = true;
    }
    RefreshMethodCache();
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
            RefreshMethodCache();
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

    // Tear down the current instance's hot-reload / deletion watch.
    UnsubscribeFileWatch();

    // Drop the old Python instance and its introspected/pending state — the
    // pending values belonged to the previous script and no longer apply.
    if (Py_IsInitialized() && m_pyData) {
        py::gil_scoped_acquire gil;
        m_pyData->scriptInstance = py::object();
        RefreshMethodCache();
    }
    m_fields.clear();
    m_pendingFieldValues.clear();
    m_lastPolledValues.clear();
    m_scriptFilePath.clear();
    m_missing = false;

    moduleName = module;
    className  = cls;

    // Same sequence as Init(): instantiate → introspect → apply → init → watch.
    RebuildInstance();
    SubscribeFileWatch();

    Notify(SCRIPT_RELOADED_EVENT);
}

void ScriptComponent::IntrospectFields()
{
    auto& scriptInstance = m_pyData->scriptInstance;
    if (!scriptInstance || scriptInstance.is_none()) return;
    
    py::gil_scoped_acquire gil;
    m_fields.clear();
    // A reload can change the field set and reassigns every changeEvent id, so the
    // last-synced snapshot no longer applies — drop it and let PollFieldChanges
    // reseed from the fresh values.
    m_lastPolledValues.clear();

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

            // Guarded like the other optional keys below: a script module left
            // over from before this key existed would otherwise KeyError here and
            // take the whole field list with it.
            if (d.contains("read_only"))
                info.readOnly = d["read_only"].cast<bool>();
            if (d.contains("widget"))
                info.widget = d["widget"].cast<std::string>();

            // Read element-wise rather than through pybind11/stl.h: this TU
            // deliberately does its own list marshalling everywhere (see
            // ReadListField), and pulling in stl.h would change how every
            // container in it converts.
            if (d.contains("option_labels")) {
                for (auto label : d["option_labels"])
                    info.optionLabels.push_back(label.cast<std::string>());
            }
            if (d.contains("option_values")) {
                for (auto value : d["option_values"])
                    info.optionValues.push_back(value.cast<int>());
            }

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

            // Apply the annotated default when the instance has no value for this
            // field yet, OR when it holds a value whose type is incompatible with
            // the annotated type. The latter happens when a field's type
            // annotation was edited and the old value was carried across a
            // hot-reload: the annotation wins, so the field takes on the newly
            // declared type and resets to its default. (Previously the stale value
            // "won" and silently changed the field's displayed type — and a
            // list-valued field left annotated as a scalar made the inspector cast
            // a list to an int, throwing an uncaught pybind error that crashed.)
            bool applyDefault = !py::hasattr(scriptInstance, info.name.c_str());
            if (!applyDefault) {
                py::object cur = py::getattr(scriptInstance, info.name.c_str());
                applyDefault = !ValueMatchesFieldType(cur, info.typeName, info.elementTypeName);
            }
            if (applyDefault) {
                py::object defaultVal = d["default"];
                if (!defaultVal.is_none()) {
                    // For unassigned sprite/material/component refs, set None so
                    // the field starts empty rather than as a handler bound to an
                    // empty id (and handler setters' `if value is not None`
                    // guards behave correctly).
                    const bool isRef = info.refTypeName == "sprite"
                                    || info.refTypeName == "material"
                                    || info.refTypeName.rfind("component:", 0) == 0
                                    || info.refTypeName.rfind("gameobject:", 0) == 0;
                    if (isRef
                        && py::isinstance<py::str>(defaultVal)
                        && defaultVal.cast<std::string>().empty()) {
                        py::setattr(scriptInstance, info.name.c_str(), py::none());
                    } else {
                        py::setattr(scriptInstance, info.name.c_str(), defaultVal);
                    }
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
                } else if (field.refTypeName.rfind("gameobject:", 0) == 0) {
                    // GameObject ref: strVal is the object's id; wrap it in a
                    // GameObject handler (empty → None). The ":<ClassName>" suffix
                    // only filters the editor picker, so it is ignored here.
                    py::module_ intro = py::module_::import("Domain.lib.utils.introspection");
                    py::setattr(scriptInstance, field.name.c_str(),
                                intro.attr("make_gameobject_ref")(strVal));
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
                const int elemWidth = VecWidth(field.elementTypeName);
                py::list lst;
                if (val.IsSequence()) {
                    for (const auto& elNode : val) {
                        if (elemWidth > 0) {
                            // Nested sequence of components, as Serialize wrote it.
                            // A short or malformed entry keeps its leading
                            // components and zeroes the rest rather than dropping
                            // the whole row and silently shortening the list.
                            auto comps = elNode.IsSequence() ? elNode.as<std::vector<float>>()
                                                             : std::vector<float>{};
                            glm::vec4 v(0.0f);
                            for (int i = 0; i < elemWidth && i < static_cast<int>(comps.size()); ++i)
                                v[i] = comps[i];
                            lst.append(MakeVecObject(v, elemWidth));
                        }
                        else if (field.elementTypeName == "float")
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
    catch (const std::exception& e) {
        // See GetAllFieldValues: a type-mismatched value throws a pybind
        // cast_error (a std::exception, not error_already_set). Degrade to the
        // default rather than letting it crash the app.
        std::cerr << "[ScriptComponent] Type mismatch getting field '" << name
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
        catch (const std::exception& e) {
            // A value whose type doesn't match the declared field type (e.g. a
            // stale hot-reload value) makes .cast<T>() throw a pybind cast_error,
            // which is NOT a py::error_already_set. Swallow it per-field so the
            // inspector rebuild continues instead of terminating the app.
            std::cerr << "[ScriptComponent] Type mismatch reading field '" << field.name
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
                } else if (refTypeName.rfind("gameobject:", 0) == 0) {
                    // GameObject ref: `v` is the object's id; wrap it in a GameObject
                    // handler (empty → None). See introspection.make_gameobject_ref.
                    py::module_ intro = py::module_::import("Domain.lib.utils.introspection");
                    py::setattr(scriptInstance, name.c_str(),
                                intro.attr("make_gameobject_ref")(v));
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
            } else if constexpr (std::is_same_v<T, std::vector<glm::vec2>>) {
                py::list lst;
                for (const auto& x : v) lst.append(MakeVecObject(glm::vec4(x, 0.0f, 0.0f), 2));
                py::setattr(scriptInstance, name.c_str(), lst);
            } else if constexpr (std::is_same_v<T, std::vector<glm::vec3>>) {
                py::list lst;
                for (const auto& x : v) lst.append(MakeVecObject(glm::vec4(x, 0.0f), 3));
                py::setattr(scriptInstance, name.c_str(), lst);
            } else if constexpr (std::is_same_v<T, std::vector<glm::vec4>>) {
                py::list lst;
                for (const auto& x : v) lst.append(MakeVecObject(x, 4));
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

        // Keep the poll snapshot current so this editor-driven edit doesn't look
        // like a script-side mutation on the next PollFieldChanges (which would
        // fire a redundant change event / thumbnail refresh).
        m_lastPolledValues[name] = value;
    }
    catch (const py::error_already_set& e) {
        std::cerr << "[ScriptComponent] Error setting field '" << name 
                  << "': " << e.what() << std::endl;
    }
}


void ScriptComponent::PollFieldChanges()
{
    {
        py::gil_scoped_acquire gil;
        auto& scriptInstance = m_pyData->scriptInstance;
        if (!scriptInstance || scriptInstance.is_none()) return;
    }

    // Read every field in one GIL acquisition (inside GetAllFieldValues), then
    // diff against the last values the inspector saw. GetAllFieldValues has
    // released the GIL by the time we Notify, so widget refreshes (which may touch
    // GL for thumbnails) never run under it.
    auto current = GetAllFieldValues();
    for (auto& [name, value] : current) {
        auto it = m_lastPolledValues.find(name);
        if (it == m_lastPolledValues.end()) {
            // First observation — seed without notifying; the bound widget was just
            // built from this same value.
            m_lastPolledValues.emplace(name, value);
            continue;
        }
        if (it->second != value) {
            it->second = value;
            for (const auto& f : m_fields) {
                if (f.name == name) { Notify(f.changeEvent); break; }
            }
        }
    }
}

ScriptComponent::~ScriptComponent() {
    if (Py_IsInitialized() && m_pyData) {
        py::gil_scoped_acquire gil;
        m_pyData->scriptInstance = py::object();
        RefreshMethodCache();
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

