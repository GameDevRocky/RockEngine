#pragma once
#include <vector>
#include <iostream>
#include <string>
#include "yaml-cpp/yaml.h"
#include "engine/serialization/Serializable.hpp"
#include "engine/core/GameObject.hpp"

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
    std::string GetTypeName() override {return "Scene";}
    std::string GetName(){return name;}

    void SetName(std::string name){this->name = name;}
    std::vector<GameObject*> GetGameObjects() {return gameobjects;}
protected:
    bool active = true;
    bool dirty = false;

private:
    std::string name;  
    std::vector<GameObject*> gameobjects;
};
