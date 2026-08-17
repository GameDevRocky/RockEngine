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
    std::vector<int>, std::vector<float>, std::vector<bool>, std::vector<std::string>,
    std::vector<glm::vec2>, std::vector<glm::vec3>, std::vector<glm::vec4>>;

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
    // For "list" fields only: the element type (any scalar typeName above) and,
    // when the element is an asset ref, its ref type ("material"/"sprite"/...).
    //
    // Everything below this line describes the ELEMENT when typeName == "list",
    // because that is where a list's Reflect metadata is written:
    // list[Reflect[float, Range(0, 1), Slider()]] annotates one row, not the
    // list. Introspection validates it against elementTypeName for exactly that
    // reason, so the editor can read min/max/step/widget/options the same way
    // whether it is building a field or a row inside one.
    std::string elementTypeName;
    std::string elementRefTypeName;
    float min = -std::numeric_limits<float>::max();
    float max =  std::numeric_limits<float>::max();
    float step = 0.1f;
    std::string tooltip;
    // Display-only in the inspector (Reflect[T, ReadOnly()]). The field is still
    // read and still refreshes as the script mutates it — only editing is blocked.
    bool readOnly = false;
    // Reflect[T, Options(...)] — dropdown choices, in display order. Empty for an
    // ordinary field. Only "int" and "str" fields can carry them; introspection
    // rejects the rest. optionValues is parallel to optionLabels and is what an
    // int field stores (defaulting to the index); a str field stores the label
    // itself, so the values are unused there.
    std::vector<std::string> optionLabels;
    std::vector<int> optionValues;
    // Reflect[T, Slider()] / RangeSlider() — which editor to draw. "" is the
    // default one for the type. Introspection only sets it once it has checked the
    // field type and that a Range exists, so the editor can trust it.
    //   "slider"       — float, dragged along [min, max]
    //   "range_slider" — vec2 (x = low, y = high) spanning [min, max]
    std::string widget;
    Observable::Event changeEvent = 0;
};

// A Python method the script marked @action: a button in the inspector, and a
// target for an event call entry. At most one argument, typed from the method's
// own annotation — `def hit(self, amount: float)` gets a float field.
struct ScriptActionInfo {
    std::string name;      // the Python method name
    std::string label;     // display text (the @action label, else a prettified name)
    std::string tooltip;   // the method's docstring first line, unless overridden
    bool hasArg = false;
    std::string argName;
    std::string argTypeName;     // same vocabulary as ScriptFieldInfo::typeName
    std::string argRefTypeName;  // "sprite"/"material"/"gameobject:X"/"component:Y"
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

    // True when a script IS assigned but its class could not be resolved — the .py
    // file was deleted or renamed, or the class was removed from it. Distinct from
    // an unassigned component (empty module/class), which is not an error state.
    //
    // A missing script keeps its module/class and its last known field values, and
    // still serializes both, so restoring the file restores the component exactly
    // (the file watcher does it live). SCRIPT_RELOADED_EVENT fires on every
    // transition into and out of this state, so the inspector reflects it.
    bool IsScriptMissing() const { return m_missing; }

    // Absolute path of the .py file backing this script — the resolved module file
    // when it loaded, otherwise where the module is expected to live. Empty only
    // when no script is assigned.
    std::string GetScriptFilePath() const { return m_scriptFilePath; }

    // Enumerate every ScriptableComponent subclass under the sandbox scripts
    // folder (for the inspector's script picker). No instance required.
    static std::vector<ScriptClassInfo> GetAvailableScripts();

    // Reassign which Python script this component runs, live: tears down the old
    // instance, re-instantiates + re-introspects the new class, and fires
    // SCRIPT_RELOADED_EVENT so the inspector rebuilds.
    void SetScript(const std::string& module, const std::string& cls);

    // Methods the script decorated with @action. Rebuilt on every (re)introspection,
    // so a hot-reload that adds or renames one is picked up like a field change.
    const std::vector<ScriptActionInfo>& GetScriptActions() const { return m_actions; }

    // Call one, with its single argument in serialized string form (see
    // ComponentActions::Invoke for the encoding; ignored when the action takes
    // none). Returns false and fills `error` if the method is missing or the
    // argument will not convert. Python exceptions are caught and reported, never
    // propagated — a broken action must not take the frame down with it.
    bool InvokeAction(const std::string& name, const std::string& rawArg,
                      std::string* error = nullptr);

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
    void RefreshMethodCache(); // re-resolve cached lifecycle methods; caller holds the GIL
    void IntrospectFields();
    void IntrospectActions();
    void ApplyPendingFields();
    void SubscribeFileWatch();   // (re)subscribe hot-reload watch for m_scriptFilePath
    void UnsubscribeFileWatch();
    // instantiate -> introspect -> apply pending -> init(). The shared tail of
    // Init(), SetScript(), and recovery from the missing state.
    void RebuildInstance();
    // Enter the missing state: snapshot the live field values as pending (so they
    // survive and can be restored), drop the Python instance, notify.
    void MarkMissing();
    // Read every field off the live instance into m_pendingFieldValues, in the same
    // YAML shape Deserialize() feeds it. No-op without an instance.
    void CaptureFieldsAsPending();
    void CallMethod(const char* funcName);
    void CallMethodStr(const char* funcName, const char* arg);

    std::string moduleName;
    std::string className;
    std::string m_scriptFilePath;
    bool m_missing = false;
    int m_fileWatchSubId = -1;
    int m_fileDeleteSubId = -1;
    std::unique_ptr<ScriptInstanceData> m_pyData;
    std::vector<ScriptFieldInfo> m_fields;
    std::vector<ScriptActionInfo> m_actions;
    std::map<std::string, YAML::Node> m_pendingFieldValues;
    // Last field values the inspector was synced to, keyed by field name. Seeded
    // and updated by PollFieldChanges; cleared on (re)introspection since a reload
    // can change the field set and each field's changeEvent id.
    std::map<std::string, ScriptFieldValue> m_lastPolledValues;

    
};
