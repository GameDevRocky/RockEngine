#include "engine/core/Scene.hpp"
#include <iostream>
#include "engine/core/GameObject.hpp"
#include "engine/core/Component.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/serialization/SerializableFactory.hpp"

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

void Scene::Deserialize(const YAML::Node& data) {
    Serializable::Deserialize(data);
    if (data["name"])
        name = data["name"].as<std::string>();
    else
        name = "Unnamed Scene";

    if (data["gameobjects"]) {
        for (const auto& goNode : data["gameobjects"]) {
            GameObject* obj = new GameObject();
            obj->Deserialize(goNode);
            Registry::Get().Register(obj);
        }
    } 

    if (data["components"]) {
        for (const auto& compNode : data["components"]) {
            std::string typeName = compNode["type"].as<std::string>();
            Serializable* created = SerializableFactory::Create(typeName);
            if (!created) continue;

            Component* comp = dynamic_cast<Component*>(created);
            if (!comp) {
                std::cerr << "Type " << typeName << " is not a Component!" << std::endl;
                delete created;
                continue;
            }
            comp->Deserialize(compNode);
            Registry::Get().Register(comp);
        }
    }
    Registry::Get().ResolveLinks();
    
    for (auto& [id, obj] : Registry::Get().GetAll()){
        obj->PostDeserialize();
    }

    
}
void Scene::LinkSerializables(){
    
}