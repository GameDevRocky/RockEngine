#include "engine/core/SceneManager.hpp"
#include <iostream>
#include "engine/serialization/Registry.hpp"
#include "engine/core/TimeManager.hpp"
#include "Engine.hpp"

#define SAMPLE_SCENE_PATH "Domain/scenes/SampleScene.yaml"

void SceneManager::Init(){
    

}
void SceneManager::PostInit(){
    LoadScene(SAMPLE_SCENE_PATH); 
}
void SceneManager::Update(){
    Engine* engine = Engine::Get();
    Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();
    TimeManager* timeManager = engine->GetActiveContainer()->FindSystem<TimeManager>();
    const float fixedDeltaTime = timeManager->FixedDeltaTime();
    const float frameTime = timeManager->DeltaTime();
    accumulator += frameTime;
    while (accumulator >= fixedDeltaTime){
        for(auto& scene_id : scene_ids){
        Scene* scene = registry->Find<Scene>(scene_id);
        scene->FixedUpdate();
        accumulator -= fixedDeltaTime;
        }
    }

    for(auto& scene_id : scene_ids){
        Scene* scene = registry->Find<Scene>(scene_id);
        scene->Update();
    }
    for(auto& scene_id : scene_ids){
        Scene* scene = registry->Find<Scene>(scene_id);
        scene->LateUpdate();
    }
}


Scene* SceneManager::LoadScene(const std::string& file_path){
    Engine* engine = Engine::Get();
    Registry* registry = engine->GetActiveContainer()->FindSystem<Registry>();
    std::cout << "engine get" << std::endl;
    Scene* scene = new Scene();
    YAML::Node root = YAML::LoadFile(file_path);
    scene->Deserialize(root);
    registry->Register(scene);
    scene_ids.push_back(scene->GetID());
    Notify();
    return scene;
}


void SceneManager::RemoveScene(const std::string& scene_id) {
    scene_ids.erase(std::remove(scene_ids.begin(), scene_ids.end(), scene_id), scene_ids.end());
    Notify();
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