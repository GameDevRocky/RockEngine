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


void Scene::Init() {

    if(state >= State::Initialized) return;

    registry = container->FindSystem<Registry>();
    rootobject_ids.clear();
    for (auto& pair : registry->GetAll()){
        Transform* transform = dynamic_cast<Transform*>(pair.second);
        if (transform){
            if (!transform->GetParent())
            {
                rootobject_ids.push_back(transform->GetGameObject()->GetID());
            }
        }
    }

    std::cout << "Initializing Scene: " << name << std::endl;
    for (auto& root : GetRootObjects()) {
        root->recurseTopDown([&](auto* obj){ obj->Init();});
    }
    state = State::Initialized;
}

void Scene::PostInit() {
    if(state >= State::PostInitialized) return;

    registry = container->FindSystem<Registry>();

    std::cout << "Initializing Scene: " << name << std::endl;
    for (auto& root : GetRootObjects()) {
        root->recurseTopDown([&](auto* obj){ obj->Init();});
    }
    state = State::PostInitialized;
}

void Scene::Awake() {
    if(state >= State::Awakened) return;

    std::cout << "Awaking Scene: " << name << std::endl;
    for (auto& root : GetRootObjects()) {
        root->recurseTopDown([&](GameObject* obj){ obj->Awake();});
    }

    state = State::Awakened;
}

void Scene::Start() {
    if(state >= State::Started) return;

    std::cout << "Starting Scene: " << name << std::endl;
    for (auto& root : GetRootObjects()) {
        root->recurseTopDown([&](GameObject* obj){ obj->Start();});
    }

    state = State::Started;
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
    if(state >= State::Loaded) return;

    Serializable::Deserialize(data);
    name = data["name"].as<std::string>();
    registry = container->FindSystem<Registry>();
    
    for (auto& goNode : data["gameobjects"]) {
        GameObject* obj = new GameObject();
        obj->Attach(container);
        obj->Deserialize(goNode);
        registry->Register(obj);
    }
    
    for (auto& compNode : data["components"]) {
        std::string typeName = compNode["type"].as<std::string>();
        auto* created = SerializableFactory::Create(typeName);
        Component* comp = dynamic_cast<Component*>(created);
        comp->Attach(container);
        comp->Deserialize(compNode);
        registry->Register(comp);
    }
    state = State::Loaded;
}


void Scene::AddGameObject(GameObject* obj) { 
    registry->Register(obj);
    obj->SetScene(this->GetID());

    Transform* transform = obj->GetTransform();
    if (transform && !transform->GetParent()) {
        rootobject_ids.push_back(obj->GetID());
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
    copy->state = State::Loaded;
    return copy;
}

Scene* Scene::Copy(Container* container) {
    Scene* copy = this->Copy();
    copy->Attach(container);
    return copy;
}