#pragma once
#include <vector>
#include <algorithm>
#include "engine/core/Scene.hpp"
#include "engine/core/System.hpp"

class SceneManager : public System {
public:
    
    void Init() override;
    void PostInit() override;
    void Update() override;

    void OnEnterPlayMode() override;
    void OnExitPlayMode() override;

    void LoadScene(const std::string& file_path);
    void RemoveScene(const std::string& scene_id);

    std::vector<Scene*> GetScenes() const;

    SceneManager() = default;
    ~SceneManager() = default;

    SceneManager* Copy() override;

private:
    std::vector<std::string> scene_ids;
    float accumulator = 0.0f;
};
