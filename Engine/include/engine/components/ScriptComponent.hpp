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
	void OnCollisionEnter(GameObject* other) override;
	void OnCollisionExit(GameObject* other) override;
	void OnTriggerEnter(GameObject* other) override;
    void OnTriggerExit(GameObject* other) override;
    void Destroy() override;

    void Shutdown() override;

    ScriptComponent* Copy() override;

    std::string GetTypeName() const override { return "ScriptComponent"; }

    ScriptComponent() = default;
    ~ScriptComponent() override;

private:
    void InstantiateScript();
    template<typename... Args>
    void CallIfExists(const char* funcName, Args&&... args) {
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
            scriptInstance.attr(funcName)(std::forward<Args>(args)...);
        }
        catch (const py::error_already_set& e) {
            std::cerr << "[ScriptComponent] Python error in "
                << funcName << ":\n"
                << e.what() << std::endl;
        }
    }

private:

    std::string moduleName;  
    std::string className;
    py::object scriptInstance;

};
