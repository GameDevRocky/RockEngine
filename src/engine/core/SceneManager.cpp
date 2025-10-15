#include "engine/core/SceneManager.hpp"

void SceneManager::AddScene(Scene* scene) {
    if (scene && std::find(scenes.begin(), scenes.end(), scene) == scenes.end()) {
        scenes.push_back(scene);
    }
}

void SceneManager::RemoveScene(Scene* scene) {
    scenes.erase(std::remove(scenes.begin(), scenes.end(), scene), scenes.end());
}

const std::vector<Scene*>& SceneManager::GetScenes() const {
    return scenes;
}
