#pragma once
#include <vector>
#include <algorithm>
#include "engine/core/Scene.hpp"
#include "engine/core/System.hpp"

class SceneManager : public System {
public:
    static SceneManager& Get() {
        static SceneManager instance;
        return instance;
    }
    void Init() override{ 
        Scene *sampleScene = new Scene();

    }
    void AddScene(Scene* scene);
    void RemoveScene(Scene* scene);
    const std::vector<Scene*>& GetScenes() const;

    SceneManager() = default;
    ~SceneManager() = default;

private:
    std::vector<Scene*> scenes;
};
