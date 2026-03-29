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

void Scene::Init()
{

    if (state >= State::Initialized)
        return;
    registry = container->FindSystem<Registry>();

    std::cout << "Initializing Scene: " << name << std::endl;
    const std::string& scene_id = GetID();

    for (auto* obj : GetAllGameObjects())
    {   
        Sync(obj);
        obj->Init();
    }

    state = State::Initialized;
}

void Scene::PostInit()
{
    if (state >= State::PostInitialized)
        return;

    std::cout << "Post Initializing Scene: " << name << std::endl;

    for (auto* obj : GetAllGameObjects())
    {
        obj->PostInit();
    }

    state = State::PostInitialized;
}

void Scene::Awake()
{
    if (state >= State::Awakened)
        return;

    std::cout << "Awaking Scene: " << name << std::endl;
    for (auto &root : GetRootObjects())
    {
        root->recurseTopDown([&](GameObject *obj)
                             { obj->Awake(); });
    }

    state = State::Awakened;
}

void Scene::Start()
{
    if (state >= State::Started)
        return;

    std::cout << "Starting Scene: " << name << std::endl;
    for (auto &root : GetRootObjects())
    {
        root->recurseTopDown([&](GameObject *obj)
                             { obj->Start(); });
    }

    state = State::Started;
}

void Scene::Update()
{
    for (auto &root : GetRootObjects())
    {
        root->recurseTopDown([&](GameObject *obj)
                             { if (obj->GetActive()) obj->Update(); });
    }
}
void Scene::FixedUpdate()
{
    for (auto &root : GetRootObjects())
    {
        root->recurseTopDown([&](GameObject *obj)
                             {  if (obj->GetActive()) obj->FixedUpdate(); });
    }
}
void Scene::LateUpdate()
{
    for (auto &root : GetRootObjects())
    {
        root->recurseTopDown([&](GameObject *obj)
                             {  if (obj->GetActive()) obj->LateUpdate(); });
    }
}

YAML::Node Scene::Serialize()
{
    YAML::Node node = Serializable::Serialize();
    node["name"] = name;
    GetRootObjects();
    node["root_objects"] = rootobject_ids;
    return node;
}

void Scene::Deserialize(const YAML::Node &data)
{
    if (state >= State::Loaded)
        return;

    Serializable::Deserialize(data);
    name = data["name"].as<std::string>();
    registry = container->FindSystem<Registry>();

    for (auto &goNode : data["gameobjects"])
    {
        GameObject* obj = new GameObject();
        obj->SetScene(this);
        obj->Attach(container);
        obj->Deserialize(goNode);
        registry->Register(obj);
        gameobject_ids.push_back(obj->GetID());
    }

    for (auto &compNode : data["components"])
    {
        std::string typeName = compNode["type"].as<std::string>();
        auto* created = SerializableFactory::Create(typeName);
        Component* comp = dynamic_cast<Component*>(created);
        comp->Attach(container);
        comp->Deserialize(compNode);
        registry->Register(comp);
    }
    state = State::Loaded;
}

void Scene::SyncRootObjects(const std::string& child_id, const std::string& parent_id){
    auto it = std::find(rootobject_ids.begin(), rootobject_ids.end(), child_id);
    bool isRoot = (it != rootobject_ids.end());
    bool changed = false;
    if (!parent_id.empty()){
        if (isRoot) {
            rootobject_ids.erase(it);
            changed = true;
        }
    } else {
        if (!isRoot) {
            rootobject_ids.push_back(child_id);
            changed = true;
        }
    }
    if (changed) Notify(HIERARCHY_CHANGED_EVENT, child_id);
}

void Scene::SyncAllObjects(const std::string& id){
    auto it = std::find(gameobject_ids.begin(), gameobject_ids.end(), id);
    if (it != gameobject_ids.end()){
        gameobject_ids.erase(it);
    }
    
    auto root_it = std::find(rootobject_ids.begin(), rootobject_ids.end(), id);
    if (root_it != rootobject_ids.end()){
        rootobject_ids.erase(root_it);
    }
}

void Scene::AddGameObject(GameObject *obj)
{
    registry->Register(obj);
    obj->SetScene(this);
    Sync(obj);

    gameobject_ids.push_back(obj->GetID());
    
    Transform* transform = obj->GetTransform();
    if (transform && !transform->GetParent()) {
        rootobject_ids.push_back(obj->GetID());
    }
}

void Scene::Sync(GameObject* obj){

    const std::string& obj_id = obj->GetID();
    const std::string& scene_id = GetID();

    obj->GetTransform()->Subscribe([obj_id, scene_id](const std::any& data){
        Scene* scene = Registry::FindInRuntime<Scene>(scene_id);
        std::string parent_id = std::any_cast<std::string>(data);
        scene->SyncRootObjects(obj_id, parent_id);
    }, Transform::PARENT_CHANGED_EVENT);  

    obj->Subscribe([obj_id, scene_id](std::any data){
        Scene* scene = Registry::FindInRuntime<Scene>(scene_id);
        scene->SyncAllObjects(obj_id);
    }, GameObject::SCENE_CHANGED_EVENT);  

    obj->Subscribe([obj_id, scene_id](std::any data){
        Scene* scene = Registry::FindInRuntime<Scene>(scene_id);
        scene->SyncAllObjects(obj_id);
    }, GameObject::SHUTDOWN_EVENT);  

}

std::vector<GameObject*> Scene::GetRootObjects()
{
    
    std::vector<GameObject*> result;
    for (auto& id : rootobject_ids)
    {   
        GameObject* obj = this->registry->Find<GameObject>(id);
        if (obj)
        {
            result.push_back(obj);
        }
    }
    return result;
}

std::vector<GameObject*> Scene::GetAllGameObjects()
{

    std::vector<GameObject*> result;
    Registry* registry = container->FindSystem<Registry>();
    for (auto& id : gameobject_ids){
        GameObject* obj = registry->Find<GameObject>(id);
        if (obj) result.push_back(obj);

    }
    return result;
}

void Scene::SetName(const std::string& name){
    this->name = name;
    Notify(NAME_CHANGED_EVENT, name);
}

void Scene::Shutdown(){
    

}

Scene *Scene::Copy()
{
    Scene *copy = new Scene();
    copy->id = id;
    copy->name = name;
    copy->rootobject_ids = rootobject_ids;
    copy->gameobject_ids = gameobject_ids;
    copy->subscribers = subscribers;
    copy->state = State::Loaded;
    return copy;
}

Scene *Scene::Copy(Container *container)
{
    Scene *copy = this->Copy();
    copy->Attach(container);
    return copy;
}