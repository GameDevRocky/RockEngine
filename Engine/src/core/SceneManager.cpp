#include "engine/core/SceneManager.hpp"
#include <iostream>
#include "engine/serialization/Registry.hpp"
#include "engine/core/TimeManager.hpp"
#include "Engine.hpp"


void SceneManager::Init(){
    std::cout << "SceneManager initialized" << std::endl;
}

void SceneManager::PostInit(){
    std::cout << "SceneManager Post Initialized" << std::endl;
}

void SceneManager::Update() {
    Registry* registry = container->FindSystem<Registry>();
    TimeManager* timeManager = container->FindSystem<TimeManager>();

    const float fixedDeltaTime = timeManager->FixedDeltaTime();
    const float frameTime = timeManager->DeltaTime();

    accumulator += frameTime;

    while (accumulator >= fixedDeltaTime) {
        for (auto& scene_id : scene_ids) {
            Scene* scene = registry->Find<Scene>(scene_id);
            if (!scene) continue;
            scene->FixedUpdate();
        }

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


void SceneManager::OnEnterPlayMode(){
    Registry* registry = container->FindSystem<Registry>();
    for(auto& scene_id : scene_ids){
        Scene* scene = registry->Find<Scene>(scene_id);
        if (!scene) continue;
        scene->OnEnterPlayMode();
    }

}

void SceneManager::OnExitPlayMode(){
    Registry* registry = container->FindSystem<Registry>();
    for(auto& scene_id : scene_ids){
        Scene* scene = registry->Find<Scene>(scene_id);
        if (!scene) continue;
        scene->OnExitPlayMode();
    }
}

void SceneManager::LoadScene(const std::string& file_path){
    std::cout << "Loading scene from path: " + file_path << std::endl;

    Registry* registry = container->FindSystem<Registry>();
    Scene* scene = new Scene();

    YAML::Node root = YAML::LoadFile(file_path);
    
    scene->Attach(container);
    std::cout << "Deserializing scene from path: " + file_path << std::endl;
    scene->Deserialize(root);
    registry->Register(scene);
    std::cout << "Initializing scene from path: " + file_path << std::endl;

    if (container->GetMode() == Container::Mode::Runtime){
        scene->OnEnterPlayMode();
    } 
    else{
        scene->Init();
        scene->PostInit();
        scene->Awake();
    }

    scene_ids.push_back(scene->GetID());
    std::cout << "Completed loading scene from path: " + file_path << std::endl;
}


void SceneManager::RemoveScene(const std::string& scene_id) {
    
}

std::vector<Scene*> SceneManager::GetScenes() const {
    Engine* engine = Engine::Get();
    Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();
    std::vector<Scene*> scenes;
    for (auto& s_id : scene_ids){
        Scene* scene = registry->Find<Scene>(s_id);
        scenes.push_back(scene);
    }
    return scenes;
}

SceneManager* SceneManager::Copy(){
    SceneManager* copy = new SceneManager();
    copy->scene_ids = scene_ids;
    copy->accumulator = 0;
    return copy;
    
}

SceneManager* SceneManager::Copy(Container* container){
    SceneManager* copy = this->Copy();
    copy->Attach(container);
    return copy;
}


