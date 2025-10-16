#include "engine/core/SceneManager.hpp"

void SceneManager::Init(){
    Scene *scene = new Scene("Untitled");
    AddScene(scene);
}

void SceneManager::AddScene(Scene* scene) {
    scenes.push_back(scene);
}

void SceneManager::RemoveScene(Scene* scene) {
    scenes.erase(std::remove(scenes.begin(), scenes.end(), scene), scenes.end());
}

const std::vector<Scene*>& SceneManager::GetScenes() const {
    return scenes;
}
