#pragma once

#include "engine/components/Component.hpp"

#include <string>
#include <vector>
#include <map>
#include <variant>
#include <memory>
#include <glm/glm.hpp>

// Windows.h (pulled in via Python.h / pybind11) defines CreateEvent as a macro.
// Undefine it here so Observable::CreateEvent() compiles correctly.
#ifdef CreateEvent
#undef CreateEvent
#endif

// Variant type for script field values — no pybind11 types exposed.
// Scalars plus homogeneous lists (list[T]); ref lists (sprite/material) marshal
// as std::vector<std::string> of IDs, like the scalar str-ref path.
using ScriptFieldValue = std::variant<
    float, int, bool, std::string, glm::vec2, glm::vec3, glm::vec4,
    std::vector<int>, std::vector<float>, std::vector<bool>, std::vector<std::string>>;

// A user script class available to attach to a ScriptComponent, identified by
// its Python module (file stem) and class name. Pybind-free, editor-facing.
struct ScriptClassInfo {
    std::string moduleName;
    std::string className;
};

struct ScriptFieldInfo {
    std::string name;
    std::string typeName;      // "float", "int", "bool", "str", "vec2", "vec3", "vec4", "list"
    std::string refTypeName;   // For str fields: "material", "sprite", "gameobject:<ClassName>" (empty = plain string)
    // For "list" fields only: the element type ("float"/"int"/"bool"/"str") and,
    // when the element is an asset ref, its ref type ("material"/"sprite"/...).
    std::string elementTypeName;
    std::string elementRefTypeName;
    float min = -std::numeric_limits<float>::max();
    float max =  std::numeric_limits<float>::max();
    float step = 0.1f;
    std::string tooltip;
    Observable::Event changeEvent = 0;
};

// Forward-declared opaque type hiding pybind11
struct ScriptInstanceData;

class ScriptComponent : public Component {
public:

    static inline const Event SCRIPT_RELOADED_EVENT = Observable::CreateEvent();

    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;

    void Init() override;    
    void Awake() override;
    void Start() override;
    void Update() override;
    void FixedUpdate() override;
    void LateUpdate() override;
	void OnCollisionEnter(GameObject* other) override;
	void OnCollisionExit(GameObject* other) override;
	void OnTriggerEnter(GameObject* other) override;
    void OnTriggerExit(GameObject* other) override;
    void Shutdown() override;
    
    void Accept(IVisitor* v) override;

    ScriptComponent* Copy() override;

    std::string GetTypeName() const override { return "ScriptComponent"; }
    std::string GetScriptModuleName() const { return moduleName; }
    std::string GetScriptClassName() const { return className; }

    // Enumerate every ScriptableComponent subclass under the sandbox scripts
    // folder (for the inspector's script picker). No instance required.
    static std::vector<ScriptClassInfo> GetAvailableScripts();

    // Reassign which Python script this component runs, live: tears down the old
    // instance, re-instantiates + re-introspects the new class, and fires
    // SCRIPT_RELOADED_EVENT so the inspector rebuilds.
    void SetScript(const std::string& module, const std::string& cls);

    // Exposed field introspection API (no pybind11 types in interface)
    const std::vector<ScriptFieldInfo>& GetFields() const { return m_fields; }
    ScriptFieldValue GetFieldValue(const std::string& name);
    std::map<std::string, ScriptFieldValue> GetAllFieldValues();
    void SetFieldValue(const std::string& name, const ScriptFieldValue& value);

    // Detect fields the Python script mutated by direct attribute assignment
    // (self.x = ...), which — unlike SetFieldValue — fires no change event. Reads
    // the current values, and for each one that differs from the last observed
    // value fires the field's changeEvent so a bound inspector widget refreshes
    // through the normal subscription. Cheap no-op when nothing changed; the
    // editor calls this on a timer only for the currently-inspected component(s).
    void PollFieldChanges();

    void ApplyHotReload();

    ScriptComponent();
    ~ScriptComponent() override;

private:
    void InstantiateScript();
    void IntrospectFields();
    void ApplyPendingFields();
    void SubscribeFileWatch();   // (re)subscribe hot-reload watch for m_scriptFilePath
    void CallMethod(const char* funcName);
    void CallMethodStr(const char* funcName, const char* arg);

    std::string moduleName;  
    std::string className;
    std::string m_scriptFilePath;
    int m_fileWatchSubId = -1;
    std::unique_ptr<ScriptInstanceData> m_pyData;
    std::vector<ScriptFieldInfo> m_fields;
    std::map<std::string, YAML::Node> m_pendingFieldValues;
    // Last field values the inspector was synced to, keyed by field name. Seeded
    // and updated by PollFieldChanges; cleared on (re)introspection since a reload
    // can change the field set and each field's changeEvent id.
    std::map<std::string, ScriptFieldValue> m_lastPolledValues;

    
};
