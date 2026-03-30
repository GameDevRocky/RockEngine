#include "engine/core/SceneManager.hpp"
#include <iostream>
#include "engine/serialization/Registry.hpp"
#include "engine/core/TimeManager.hpp"
#include "engine/core/PhysicsSystem.hpp"
#include "Engine.hpp"
#include "engine/utils/EngineUtils.hpp"
#include <filesystem> // <--- ADD THIS LINE
#include "engine/debug/Console.hpp"

using namespace EngineUtils;
void SceneManager::Init(){

    registry = container->FindSystem<Registry>();
    timeManager = container->FindSystem<TimeManager>();
    physicsSystem = container->FindSystem<PhysicsSystem>();
    if (state >= State::Initialized) return ;
    for (auto* scene : GetScenes()){
        scene->Init();
    }
    state = State::Initialized;
    std::cout << "SceneManager Initialized" << std::endl;
}
void SceneManager::PostInit(){

    if (state >= State::PostInitialized) return ;
    for (auto* scene : GetScenes()){
        scene->PostInit();
    }
    state = State::PostInitialized;
    std::cout << "SceneManager Post Initialized" << std::endl;
}
void SceneManager::Awake(){

    if (state >= State::Awakened) return ;
    for (auto* scene : GetScenes()){
        scene->Awake();
    }
    state = State::Awakened;
    std::cout << "SceneManager Awakened" << std::endl;
}
void SceneManager::Start(){

    if (state >= State::Started) return ;
    for (auto* scene : GetScenes()){
        scene->Start();
    }
    state = State::Started;
    std::cout << "SceneManager Started" << std::endl;
}

void SceneManager::Update() { 
    const float fixedDeltaTime = timeManager->FixedDeltaTime();
    const float frameTime = timeManager->DeltaTime();

    accumulator += frameTime;

    while (accumulator >= fixedDeltaTime) {
        for (auto& scene_id : scene_ids) {
            Scene* scene = registry->Find<Scene>(scene_id);
            if (!scene){
                std::cout << "Error in Scene Manager, scene is null" << std::endl;
            }
            scene->FixedUpdate();
        }
        physicsSystem->Step();
        accumulator -= fixedDeltaTime; 
    }

    for (auto& scene_id : scene_ids) {
        Scene* scene = registry->Find<Scene>(scene_id);
        if (!scene) continue;
        scene->Update();
    }

    for (auto& scene_id : scene_ids) {
        Scene* scene = registry->Find<Scene>(scene_id);
        if (!scene) continue;
        scene->LateUpdate();
    }
}


void SceneManager::LoadScene(const std::string& file_path){
    std::string finalPath = file_path;

    std::filesystem::path p(file_path);
    if (!p.is_absolute()) {
        finalPath = GetAssetPath(file_path);
    }

    if (!std::filesystem::exists(finalPath)) {
        return;
    }
    std::cout << "Loading scene from path: " + file_path << std::endl;
    YAML::Node root = YAML::LoadFile(finalPath);
    
    if (root["id"]) {
        std::string id = root["id"].as<std::string>();
        if (std::find(scene_ids.begin(), scene_ids.end(), id) != scene_ids.end()) {
            std::cout << "Scene already loaded: " + id << std::endl;
            return;
        }
    }

    Scene* scene = new Scene();
    scene->Attach(container);

    std::cout << "Deserializing scene from path: " + file_path << std::endl;
    scene->Deserialize(root);
    registry->Register(scene);
    
    std::cout << "Initializing scene from path: " + file_path << std::endl;
    scene->Init();
    scene->PostInit();
    
    if (container->GetMode() == Container::Mode::Runtime){
        std::cout << "Awakening scene (Runtime mode): " + file_path << std::endl;
        scene->Awake();
        scene->Start();
    }

    scene_ids.push_back(scene->GetID());
    std::cout << "Completed loading scene from path: " + file_path << std::endl;
    Notify(LOADED_SCENE_EVENT, scene->GetID());
}


void SceneManager::RemoveScene(const std::string& scene_id) {
    
}

std::vector<Scene*> SceneManager::GetScenes() const {
    std::vector<Scene*> scenes;
    for (auto& s_id : scene_ids){
        Scene* scene = registry->Find<Scene>(s_id);
        scenes.push_back(scene);
    }
    return scenes;
}

void SceneManager::Shutdown(){
   
}

SceneManager* SceneManager::Copy(){
    SceneManager* copy = new SceneManager();
    copy->scene_ids = scene_ids;
    copy->accumulator = 0;
    copy->subscribers = subscribers;
    return copy;
    
}

SceneManager* SceneManager::Copy(Container* container){
    SceneManager* copy = this->Copy();
    copy->Attach(container);
    return copy;
}


