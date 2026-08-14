#include "engine/core/Scene.hpp"
#include <iostream>
#include "engine/core/GameObject.hpp"
#include "engine/components/Component.hpp"
#include "engine/components/Transform.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/serialization/SerializableFactory.hpp"
#include "engine/serialization/IdRemapper.hpp"
#include "engine/debug/Console.hpp"
#include "engine/utils/IVisitor.hpp"
#include <algorithm>
#include <functional>
#include <unordered_set>
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
                             { if (!obj->IsMarkedForDestroy() && obj->GetActive()) obj->Update(); });
    }
}
void Scene::FixedUpdate()
{
    for (auto &root : GetRootObjects())
    {
        root->recurseTopDown([&](GameObject *obj)
                             { if (!obj->IsMarkedForDestroy() && obj->GetActive()) obj->FixedUpdate(); });
    }
}
void Scene::LateUpdate()
{
    for (auto &root : GetRootObjects())
    {
        root->recurseTopDown([&](GameObject *obj)
                             { if (!obj->IsMarkedForDestroy() && obj->GetActive()) obj->LateUpdate(); });
    }
}

void Scene::EmitSubtree(GameObject* root, YAML::Node& gameobjects, YAML::Node& components,
                        std::unordered_set<std::string>& visited)
{
    if (!root || visited.count(root->GetID())) return;
    visited.insert(root->GetID());
    gameobjects.push_back(root->Serialize());
    for (auto* comp : root->GetAllComponents())
        if (comp) components.push_back(comp->Serialize());
    if (Transform* t = root->GetTransform())
        for (Transform* child : t->GetChildren())
            if (child) EmitSubtree(child->GetGameObject(), gameobjects, components, visited);
}

YAML::Node Scene::Serialize()
{
    YAML::Node node;
    node["name"] = name;
    node["id"] = GetID();

    YAML::Node components(YAML::NodeType::Sequence);
    YAML::Node gameobjects(YAML::NodeType::Sequence);

    // Emit hierarchically (roots in order, each object's children in order) so the
    // sibling ordering is reproduced on load — the loader rebuilds root/child order
    // from the sequence in which objects are deserialized.
    std::unordered_set<std::string> visited;
    for (GameObject* root : GetRootObjects()) EmitSubtree(root, gameobjects, components, visited);
    for (GameObject* obj : GetAllGameObjects()) EmitSubtree(obj, gameobjects, components, visited); // safety: any orphans

    node["components"] = components;
    node["gameobjects"] = gameobjects;
    return node;
}

void Scene::ReorderObject(const std::string& id, const std::string& parentId, int targetIndex)
{
    GameObject* obj = registry->Find<GameObject>(id);
    if (!obj) return;
    Transform* t = obj->GetTransform();
    if (!t) return;

    Transform* curParent = t->GetParent();
    std::string curParentId = (curParent && curParent->GetGameObject())
        ? curParent->GetGameObject()->GetID() : "";

    // Reparent first if dropping under a different parent. SetParent updates
    // parent_id, rootobject_ids and the parents' children_ids (appending at the end).
    if (curParentId != parentId) {
        Transform* newParent = nullptr;
        if (!parentId.empty()) {
            if (GameObject* pObj = registry->Find<GameObject>(parentId))
                newParent = pObj->GetTransform();
        }
        t->SetParent(newParent, true);
    }

    // Place at the requested sibling index.
    if (parentId.empty()) {
        auto it = std::find(rootobject_ids.begin(), rootobject_ids.end(), id);
        if (it != rootobject_ids.end()) {
            int oldIndex = (int)std::distance(rootobject_ids.begin(), it);
            rootobject_ids.erase(it);
            if (oldIndex < targetIndex) targetIndex--;
            targetIndex = std::max(0, std::min(targetIndex, (int)rootobject_ids.size()));
            rootobject_ids.insert(rootobject_ids.begin() + targetIndex, id);
            Registry::BumpGeneration(); // root order changed
        }
    } else {
        if (GameObject* pObj = registry->Find<GameObject>(parentId)) {
            if (Transform* pt = pObj->GetTransform())
                pt->MoveChild(id, targetIndex); // MoveChild bumps the generation
        }
    }

    Notify(ORDER_CHANGED_EVENT, id);
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
    if (changed) {
        Registry::BumpGeneration(); // rootobject_ids order changed
        Notify(HIERARCHY_CHANGED_EVENT, child_id);
    }
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
    Registry::BumpGeneration(); // object/root id lists changed
}

void Scene::AddGameObject(GameObject *obj)
{
    obj->Attach(container);
    if (!obj->GetTransform()){
        auto* transform = new Transform();
        transform->SetGameObject(obj);
        obj->AddComponent(transform);
    }
        
    registry->Register(obj);
    obj->SetScene(this);
    Sync(obj);

    gameobject_ids.push_back(obj->GetID());
    obj->Init();
    obj->PostInit();

    if (container->GetMode() == Container::Mode::Runtime){
        obj->Awake();
        obj->Start();
    }

    Transform* transform = obj->GetTransform();
    if (transform && !transform->GetParent()) {
        rootobject_ids.push_back(obj->GetID());
    }
    Registry::BumpGeneration(); // id lists finalized — refresh object caches
    Notify(GAMEOBJECT_ADDED_EVENT, obj->GetID());
}

std::string Scene::MakeUniqueSiblingName(GameObject* source)
{
    // Strip an existing " (N)" suffix so duplicating "Player (1)" yields
    // "Player (2)", not "Player (1) (1)".
    std::string base = source->GetName();
    int sourceIndex = 0;
    if (!base.empty() && base.back() == ')') {
        size_t open = base.rfind(" (");
        if (open != std::string::npos) {
            std::string digits = base.substr(open + 2, base.size() - open - 3);
            if (!digits.empty() &&
                digits.find_first_not_of("0123456789") == std::string::npos) {
                sourceIndex = std::stoi(digits);
                base = base.substr(0, open);
            }
        }
    }

    // Siblings = children of the same parent, or the scene roots.
    std::vector<GameObject*> siblings;
    Transform* t = source->GetTransform();
    Transform* parent = t ? t->GetParent() : nullptr;
    if (parent) {
        for (Transform* child : parent->GetChildren())
            if (child && child->GetGameObject()) siblings.push_back(child->GetGameObject());
    } else {
        siblings = GetRootObjects();
    }

    int highest = sourceIndex;
    for (GameObject* sibling : siblings) {
        if (!sibling) continue;
        const std::string& n = sibling->GetName();
        if (n == base) continue;                       // the un-suffixed original
        if (n.size() < base.size() + 4) continue;      // shortest match is base + " (1)"
        if (n.compare(0, base.size(), base) != 0) continue;
        if (n.compare(base.size(), 2, " (") != 0 || n.back() != ')') continue;
        std::string digits = n.substr(base.size() + 2, n.size() - base.size() - 3);
        if (digits.empty() || digits.find_first_not_of("0123456789") != std::string::npos) continue;
        highest = std::max(highest, std::stoi(digits));
    }

    return base + " (" + std::to_string(highest + 1) + ")";
}

GameObject* Scene::InstantiateSubtree(const YAML::Node& gameobjects, const YAML::Node& components)
{
    if (!registry || gameobjects.size() == 0) return nullptr;

    
    std::vector<GameObject*> created;
    created.reserve(gameobjects.size());
    for (const auto& goNode : gameobjects) {
        auto* obj = new GameObject();
        obj->Attach(container);
        obj->SetScene(this);
        obj->Deserialize(goNode);
        registry->Register(obj);
        gameobject_ids.push_back(obj->GetID());
        created.push_back(obj);
    }

    for (const auto& compNode : components) {
        auto* createdComp = SerializableFactory::Create(compNode["type"].as<std::string>());
        auto* comp = dynamic_cast<Component*>(createdComp);
        if (!comp) { delete createdComp; continue; }
        comp->Attach(container);
        comp->Deserialize(compNode);
        registry->Register(comp);
    }

    // Lifecycle, top-down. Sync() must run before Init() so the PARENT_CHANGED
    // subscription is live when Transform::Init -> SetParent fires and
    // SyncRootObjects maintains rootobject_ids.
    for (auto* obj : created) {
        Sync(obj);
        obj->Init();
        obj->PostInit();
        if (container->GetMode() == Container::Mode::Runtime) {
            obj->Awake();
            obj->Start();
        }
    }

    GameObject* newRoot = created.front();
    Transform* rootTransform = newRoot->GetTransform();
    if (rootTransform && !rootTransform->GetParent()) {
        // Guarded because SetParent(nullptr) during Init may already have routed
        // through SyncRootObjects.
        const std::string& newId = newRoot->GetID();
        if (std::find(rootobject_ids.begin(), rootobject_ids.end(), newId) == rootobject_ids.end())
            rootobject_ids.push_back(newId);
    }

    Registry::BumpGeneration(); // id lists finalized — refresh object caches
    Notify(GAMEOBJECT_ADDED_EVENT, newRoot->GetID());
    return newRoot;
}

YAML::Node Scene::SnapshotSubtree(GameObject* root)
{
    YAML::Node snapshot;
    YAML::Node gameobjects(YAML::NodeType::Sequence);
    YAML::Node components(YAML::NodeType::Sequence);
    std::unordered_set<std::string> visited;
    EmitSubtree(root, gameobjects, components, visited);
    snapshot["gameobjects"] = gameobjects;
    snapshot["components"] = components;
    return snapshot;
}

GameObject* Scene::RestoreSubtree(const YAML::Node& snapshot,
                                  const std::string& parentId, int siblingIndex)
{
    if (!registry || !snapshot) return nullptr;

    const YAML::Node gameobjects = snapshot["gameobjects"];
    const YAML::Node components = snapshot["components"];
    if (!gameobjects || gameobjects.size() == 0) return nullptr;

    // Ids are preserved verbatim, so restoring twice would register duplicates.
    const std::string rootId = gameobjects[0]["id"].as<std::string>();
    if (registry->Find<GameObject>(rootId)) return nullptr;

    // Note this deliberately does not route through Deserialize(), which
    // early-returns once the scene is Loaded and would silently do nothing.
    GameObject* root = InstantiateSubtree(gameobjects, components);
    if (!root) return nullptr;

    // ReorderObject restores both halves of the placement in one call: it
    // reparents first (SetParent always appends) and then indexes.
    ReorderObject(root->GetID(), parentId, siblingIndex);
    return root;
}

GameObject* Scene::DuplicateGameObject(GameObject* source, YAML::Node* outSnapshot)
{
    if (!source || !registry || !registry->Find<GameObject>(source->GetID())) return nullptr;

    // 1. Snapshot the subtree in the same shape Deserialize() consumes. The root
    //    is emitted first and the walk is top-down — both relied on below.
    YAML::Node gameobjects(YAML::NodeType::Sequence);
    YAML::Node components(YAML::NodeType::Sequence);
    std::unordered_set<std::string> visited;
    EmitSubtree(source, gameobjects, components, visited);
    if (gameobjects.size() == 0) return nullptr;

    // 2. Give everything in the snapshot a new identity. Only ids *inside* the
    //    subtree are in the map, so the root's parent_id, asset ids, and any
    //    reference to an object elsewhere in the scene survive untouched — which
    //    is exactly what makes the clone a sibling of the source.
    auto idMap = IdRemapper::BuildMap(gameobjects, components);
    IdRemapper::Apply(gameobjects, idMap);
    IdRemapper::Apply(components, idMap);

    gameobjects[0]["name"] = MakeUniqueSiblingName(source);

    // 3. Hand back the post-remap YAML so a caller (the undo stack) can rebuild
    //    this exact clone later instead of duplicating afresh with new ids.
    if (outSnapshot) {
        (*outSnapshot)["gameobjects"] = gameobjects;
        (*outSnapshot)["components"] = components;
    }

    return InstantiateSubtree(gameobjects, components);
}

void Scene::Sync(GameObject* obj){

    if (!obj) return; // Safety check #1

    auto* transform = obj->GetTransform();

    const std::string& obj_id = obj->GetID();
    const std::string& scene_id = GetID();

    transform->Subscribe([obj_id, scene_id](const std::any& data){
        Scene* scene = Registry::FindInRuntime<Scene>(scene_id);
        if (!scene) return false;
        std::string parent_id = std::any_cast<std::string>(data);
        scene->SyncRootObjects(obj_id, parent_id);
        return true;
    }, Transform::PARENT_CHANGED_EVENT);  

    obj->Subscribe([obj_id, scene_id](std::any data){
        Scene* scene = Registry::FindInRuntime<Scene>(scene_id);
        if (!scene) return false;
        scene->SyncAllObjects(obj_id);
        return true;
    }, GameObject::SCENE_CHANGED_EVENT);  

    obj->Subscribe([obj_id, scene_id](std::any data){
        Scene* scene = Registry::FindInRuntime<Scene>(scene_id);
        if (!scene) return false;
        scene->SyncAllObjects(obj_id);
        return true;
    }, GameObject::SHUTDOWN_EVENT);  

}

const std::vector<GameObject*>& Scene::GetRootObjects()
{
    const std::uint64_t gen = Registry::Generation();
    if (cachedRootsGen == gen) return cachedRoots;

    cachedRoots.clear();
    Registry* reg = registry ? registry : (container ? container->FindSystem<Registry>() : nullptr);
    if (!reg) return cachedRoots; // pre-Init: leave unstamped so it re-resolves
    cachedRoots.reserve(rootobject_ids.size());
    for (const auto& id : rootobject_ids) {
        if (GameObject* obj = reg->Find<GameObject>(id))
            cachedRoots.push_back(obj);
    }
    cachedRootsGen = gen;
    return cachedRoots;
}

const std::vector<GameObject*>& Scene::GetAllGameObjects()
{
    const std::uint64_t gen = Registry::Generation();
    if (cachedAllGen == gen) return cachedAll;

    cachedAll.clear();
    Registry* reg = registry ? registry : (container ? container->FindSystem<Registry>() : nullptr);
    if (!reg) return cachedAll;
    cachedAll.reserve(gameobject_ids.size());
    for (const auto& id : gameobject_ids) {
        if (GameObject* obj = reg->Find<GameObject>(id))
            cachedAll.push_back(obj);
    }
    cachedAllGen = gen;
    return cachedAll;
}

void Scene::SetName(const std::string& name){
    this->name = name;
    Notify(NAME_CHANGED_EVENT, name);
}

void Scene::Shutdown(){
    for (GameObject* obj : GetRootObjects()){
        obj->recurseBottomUp([&](GameObject* obj){
            obj->Shutdown();
        });
    }
    RuntimeObject::Shutdown();
}

Scene *Scene::Copy()
{
    Scene *copy = new Scene();
    copy->id = id;
    copy->name = name;
    copy->path = path;
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

void Scene::Accept(IVisitor* v) {
    v->Visit(this);
}