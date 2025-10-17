#pragma once
#include <string>
#include <yaml-cpp/yaml.h>

class Component {
public:
    GameObject* gameobject = nullptr;
    bool enabled = true;

    virtual ~Component() = default;
    virtual void Awake(){}
    virtual void Start() {}
    virtual void Update() {}
    virtual void LateUpdate(){}
    virtual void FixedUpdate(){}
    virtual void OnDestroy() {}

    virtual std::string GetTypeName() const = 0;

    virtual YAML::Node Serialize() const = 0;

    static Component* Deserialize(const YAML::Node& node, GameObject* owner);
};
