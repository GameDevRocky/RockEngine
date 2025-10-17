#pragma once
#include <vector>
#include <iostream>
#include <string>
#include "yaml-cpp/yaml.h"
#include "engine/serialization/Serializable.hpp"

class Scene : public Serializable {
public:
    Scene() = default;
    Scene(std::string name) : name(name) {}
    ~Scene() = default;

    void Init();
    void Update();
    void Shutdown();

    YAML::Node Serialize() override;

    void Deserialize(const YAML::Node& node) override;

    std::string GetTypeName() const override {
        return "Scene";
    }
    std::string GetName(){
        return name;
    }
    void SetName(std::string name){
        this->name = name;
    }

protected:
    bool active = true;
    bool dirty = false;

private:
    std::string name;  
};
