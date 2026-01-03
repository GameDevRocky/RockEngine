#include "engine/core/Scene.hpp"
#include <iostream>
#include "engine/core/GameObject.hpp"
#include "engine/components/Component.hpp"
#include "engine/components/Transform.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/serialization/SerializableFactory.hpp"
#include "engine/debug/Console.hpp"
#include <algorithm> 

void Scene::Init() {
    std::cout << "Initializing scene: " << name << std::endl;
}

void Scene::Update() {
    for (auto& obj_id : gameobject_ids){
        GameObject* obj = Registry::Find<GameObject>(obj_id);
        obj->Update();
        obj->GetTransform()->Rotate(0.5f);

    } 
}
void Scene::FixedUpdate() {
    for (auto& obj_id : gameobject_ids){
        GameObject* obj = Registry::Find<GameObject>(obj_id);
        obj->FixedUpdate();

    } 
}
void Scene::LateUpdate() {
    for (auto& obj_id : gameobject_ids){
        GameObject* obj = Registry::Find<GameObject>(obj_id);
        obj->LateUpdate();

    } 
}

void Scene::Shutdown() {
    std::cout << "Shutting down scene: " << name << std::endl;
}

YAML::Node Scene::Serialize() {
    YAML::Node node = Serializable::Serialize();
    node["name"] = name;
    node["root_objects"] = root_object_ids;
    node["objects"] = gameobject_ids;
    return node;
}

void Scene::Deserialize(const YAML::Node& data) {
    Serializable::Deserialize(data);
    if (data["name"]) 
        name = data["name"].as<std::string>();
    else
        name = "Unnamed Scene";

    for (const auto& goNode : data["gameobjects"]) {
        GameObject* obj = new GameObject();
        obj->Deserialize(goNode);
        AddGameObject(obj);
    }

    for (const auto& compNode : data["components"]) {
        std::string typeName = compNode["type"].as<std::string>();
        Serializable* created = SerializableFactory::Create(typeName);
        Component* comp = dynamic_cast<Component*>(created);
        comp->Deserialize(compNode);
        Registry::Get().Register(comp);
    }
    
    for (auto& [id, obj] : Registry::Get().GetAll()){
        obj->PostDeserialize();
    }

    for (auto& obj : GetAllGameObjects()){    
        Transform* transform = obj->GetComponent<Transform>();
        
        if (transform && !transform->GetParent()){
            std::cout << obj->GetName() << " root " << std::endl;
            AddRootObject(obj->GetID());
        }
    }
}

void Scene::AddGameObject(GameObject* obj) {
    if (!obj)
        return;

    std::string id = obj->GetID();
    Registry::Get().Register(obj);
    if (std::find(gameobject_ids.begin(), gameobject_ids.end(), id) == gameobject_ids.end())
        gameobject_ids.push_back(id);
    obj->SetScene(GetID());
}


void Scene::AddRootObject(const std::string& obj_id) {
    if (std::find(root_object_ids.begin(), root_object_ids.end(), obj_id) != root_object_ids.end())
        return;

    root_object_ids.push_back(obj_id);
}

void Scene::RemoveRootObject(const std::string& obj_id) {
    auto it = std::find(root_object_ids.begin(), root_object_ids.end(), obj_id);
    if (it != root_object_ids.end())
        root_object_ids.erase(it);
}

std::vector<GameObject*> Scene::GetRootObjects() {
    std::vector<GameObject*> result;

    for (const std::string& id : root_object_ids) {
        if (auto* obj = dynamic_cast<GameObject*>(Registry::Find<GameObject>(id)))
            result.push_back(obj);
    }

    return result;
}

std::vector<GameObject*> Scene::GetAllGameObjects() {
    std::vector<GameObject*> result;

    for (const std::string& id : gameobject_ids) {
        if (auto* obj = Registry::Find<GameObject>(id))
            result.push_back(obj);
    }

    return result;
}
