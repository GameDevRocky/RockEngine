#include "engine/core/SceneManager.hpp"
#include <iostream>
#include "engine/serialization/Registry.hpp"
 
#define SAMPLE_SCENE_PATH "domain/scenes/SampleScene.yaml"

void SceneManager::Init(){
    LoadScene(SAMPLE_SCENE_PATH); 
}

Scene* SceneManager::LoadScene(const std::string& file_path){
    Scene* scene = new Scene();
    YAML::Node root = YAML::LoadFile(file_path);
    scene->Deserialize(root);
    Registry::Get().Register(scene);
    scene_ids.push_back(scene->GetID());
    Notify();
    return scene;
}


void SceneManager::RemoveScene(const std::string& scene_id) {
    scene_ids.erase(std::remove(scene_ids.begin(), scene_ids.end(), scene_id), scene_ids.end());
    Notify();
}

std::vector<Scene*> SceneManager::GetScenes() const {
    std::vector<Scene*> scenes;
    for (auto& s_id : scene_ids){
        Serializable* s = Registry::Get().Find(s_id);
        Scene* scene = dynamic_cast<Scene*>(s);
        scenes.push_back(scene);
    }
    return scenes;
}
