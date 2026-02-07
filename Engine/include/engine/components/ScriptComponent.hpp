#pragma once

#include "engine/components/Component.hpp"

#include <pybind11/pybind11.h>
#include <string>

namespace py = pybind11;

class ScriptComponent : public Component {
public:

    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;

    void Init() override;    
    void Awake() override;
    void Start() override;
    void Update() override;
    void FixedUpdate() override;
    void LateUpdate() override;
    void OnDestroy() override;

    ScriptComponent* Copy() override;

    std::string GetTypeName() const override { return "ScriptComponent"; }

    ScriptComponent() = default;
    ~ScriptComponent() override = default;

private:
    void InstantiateScript();
    void CallIfExists(const char* funcName);

private:

    std::string moduleName;  
    std::string className;
    py::object scriptInstance;
};
