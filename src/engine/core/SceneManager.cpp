#include "engine/core/SceneManager.hpp"
#include <iostream>
#define SAMPLE_SCENE_PATH "domain/scenes/SampleScene.yaml"



void SceneManager::Init(){
    Scene *scene = new Scene();
    YAML::Node root = YAML::LoadFile(SAMPLE_SCENE_PATH);
    scene->Deserialize(root);
    std::cout << scene->GetName() << std::endl; 
    AddScene(scene);
}

void SceneManager::AddScene(Scene* scene) {
    scenes.push_back(scene);
    Notify();
}

void SceneManager::RemoveScene(Scene* scene) {
    scenes.erase(std::remove(scenes.begin(), scenes.end(), scene), scenes.end());
}

const std::vector<Scene*>& SceneManager::GetScenes() const {
    return scenes;
}
