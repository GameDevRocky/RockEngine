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
    void Awake() override;
    void Start() override;
    void Update() override;
    void FixedUpdate() override;
    void LateUpdate() override;
    void OnDestroy() override;

    std::string GetTypeName() const override { return "ScriptComponent"; }

private:
    void InstantiateScript();
    void CallIfExists(const char* funcName);

private:
    // Serialized
    std::string moduleName;   // e.g. "player_controller"
    std::string className;    // e.g. "PlayerController"

    // Runtime-only (NOT serialized)
    py::object scriptInstance;
};
