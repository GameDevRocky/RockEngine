#pragma once

#include "engine/components/Component.hpp"

#include <pybind11/pybind11.h>
#include <string>

namespace py = pybind11;

class ScriptComponent : public Component {
public:
    ScriptComponent() = default;
    ~ScriptComponent() override = default;

    // --- Serializable ---
    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;

    // --- Lifecycle ---
    void Init() override;
    void PostInit() override;
    
    void Awake() override;
    void Update() override;
    void FixedUpdate() override;
    void LateUpdate() override;
    void OnDestroy() override;

    ScriptComponent* Copy() override;

    std::string GetTypeName() const override { return "ScriptComponent"; }

private:
    void InstantiateScript();
    void CallIfExists(const char* funcName);

private:

    std::string moduleName;  
    std::string className;
    py::object scriptInstance;
};
