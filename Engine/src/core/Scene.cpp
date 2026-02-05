#include "engine/core/Scene.hpp"
#include <iostream>
#include "engine/core/GameObject.hpp"
#include "engine/components/Component.hpp"
#include "engine/components/Transform.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/serialization/SerializableFactory.hpp"
#include "engine/debug/Console.hpp"
#include <algorithm> 
#include "Engine.hpp"


void Scene::OnEnterPlayMode() {
    if (state < SceneState::Initialized) {
        Init();
        PostInit();
    }
    
    if (state != SceneState::Active) {
        Awake();
        state = SceneState::Active;
    }
}

void Scene::Init() {
    if (state >= SceneState::Initialized) {
        std::cout << "Scene already initialized: " << name << std::endl;
        return;
    }
    registry = container->FindSystem<Registry>();

    std::cout << "Initializing Scene: " << name << std::endl;
    
    for (auto& root : GetRootObjects()) {
        root->recurseTopDown([&](auto* obj){ obj->Init();});
    }

    state = SceneState::Initialized;
}
void Scene::PostInit() {
    std::cout << "Post Initializing Scene: " << name << std::endl;
    for (auto& root : GetRootObjects()) {
        root->recurseTopDown([&](GameObject* obj){ obj->PostInit();});
    }

}
void Scene::Awake() {
    std::cout << "Awaking Scene: " << name << std::endl;
    for (auto& root : GetRootObjects()) {
        root->recurseTopDown([&](GameObject* obj){ obj->Awake();});
    }

    state = SceneState::Active;
}

void Scene::Update() {
    for (auto& root : GetRootObjects()) {
        root->recurseTopDown([&](GameObject* obj){ obj->Update();});
    }

}
void Scene::FixedUpdate() {
    for (auto& root : GetRootObjects()) {
        root->recurseTopDown([&](GameObject* obj){ obj->FixedUpdate();});
    }
}
void Scene::LateUpdate() {
    for (auto& root : GetRootObjects()) {
        root->recurseTopDown([&](GameObject* obj){ obj->LateUpdate();});
    }
}

YAML::Node Scene::Serialize() {
    YAML::Node node = Serializable::Serialize();
    node["name"] = name;
    node["root_objects"] = rootobject_ids;
    return node;
}

void Scene::Deserialize(const YAML::Node& data) {
    Serializable::Deserialize(data);
    name = data["name"].as<std::string>();
    registry = container->FindSystem<Registry>();
    
    for (const auto& goNode : data["gameobjects"]) {
        GameObject* obj = new GameObject();
        obj->Attach(container);
        obj->Deserialize(goNode);
        temp_objs.push_back(obj);

    }
    
    for (const auto& compNode : data["components"]) {
        std::string typeName = compNode["type"].as<std::string>();
        auto* created = SerializableFactory::Create(typeName);
        Component* comp = dynamic_cast<Component*>(created);
        comp->Attach(container);
        comp->Deserialize(compNode);
        temp_comps.push_back(comp);
    }
    
    state = SceneState::Deserialized;
}
void Scene::PostDeserialize() {
    for (auto* comp : temp_comps){
        comp->PostDeserialize();
        registry->Register(comp);
    }
    
    for (auto* obj : temp_objs){
        obj->PostDeserialize();
        registry->Register(obj);
        obj->SetScene(this->GetID());
        Transform* transform = obj->GetTransform();
        if (transform && !transform->GetParent()) {
            rootobject_ids.push_back(obj->GetID());
        }
    }

}

void Scene::AddGameObject(GameObject* obj) { 
    registry->Register(obj);
    obj->SetScene(this->GetID());

    Transform* transform = obj->GetTransform();
    if (transform && !transform->GetParent()) {
        rootobject_ids.push_back(obj->GetID());
    }

    if (state >= SceneState::Initialized) {
        obj->Init();
        obj->PostInit();
    }
    
    if (state >= SceneState::Active) {
        obj->Awake();
    }
}

std::vector<GameObject*> Scene::GetRootObjects() {
    std::vector<GameObject*> result;
    for (const std::string& id : rootobject_ids) {
        if (auto* obj = registry->Find<GameObject>(id))
        result.push_back(obj);
    }

    return result;
}

std::vector<GameObject*> Scene::GetAllGameObjects() {
    std::vector<GameObject*> result;
    for (auto* root : GetRootObjects()){
        root->recurseTopDown([&](GameObject* obj){
            result.push_back(obj);
        });
    }
    return result;
}

Scene* Scene::Copy(){
    Scene* copy = new Scene();
    copy->id = id;
    copy->name = name;
    copy->rootobject_ids = rootobject_ids;
    copy->state = SceneState::Deserialized;
    return copy;
}

Scene* Scene::Copy(Container* container) {
    Scene* copy = this->Copy();
    copy->Attach(container);
    return copy;
}