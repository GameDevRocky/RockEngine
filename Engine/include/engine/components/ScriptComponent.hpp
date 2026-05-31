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

// Variant type for script field values — no pybind11 types exposed
using ScriptFieldValue = std::variant<float, int, bool, std::string, glm::vec2, glm::vec3, glm::vec4>;

struct ScriptFieldInfo {
    std::string name;
    std::string typeName;   // "float", "int", "bool", "str", "vec2"
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

    // Exposed field introspection API (no pybind11 types in interface)
    const std::vector<ScriptFieldInfo>& GetFields() const { return m_fields; }
    ScriptFieldValue GetFieldValue(const std::string& name);
    std::map<std::string, ScriptFieldValue> GetAllFieldValues();
    void SetFieldValue(const std::string& name, const ScriptFieldValue& value);

    ScriptComponent();
    ~ScriptComponent() override;

private:
    void InstantiateScript();
    void IntrospectFields();
    void ApplyPendingFields();
    void CallMethod(const char* funcName);
    void CallMethodStr(const char* funcName, const char* arg);

    std::string moduleName;  
    std::string className;
    std::string m_scriptFilePath;
    int m_fileWatchSubId = -1;
    std::unique_ptr<ScriptInstanceData> m_pyData;
    std::vector<ScriptFieldInfo> m_fields;
    std::map<std::string, YAML::Node> m_pendingFieldValues;

    void ApplyHotReload();

};
