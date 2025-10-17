#include "engine/core/Scene.hpp"
#include <iostream>


void Scene::Init() {
    std::cout << "Initializing scene: " << std::endl;
}

void Scene::Update() {
    std::cout << "Updating scene" << std::endl;
}

void Scene::Shutdown() {
    std::cout << "Shutting down scene: " << std::endl;
}

YAML::Node Scene::Serialize() {
        YAML::Node node = Serializable::Serialize();
        node["name"] = name;
        return node;
    }
 void Scene::Deserialize(const YAML::Node& node){
        Serializable::Deserialize(node);
        if (node["name"]) name = node["name"].as<std::string>();
        else name = "Couldn't read name from File";
    }
