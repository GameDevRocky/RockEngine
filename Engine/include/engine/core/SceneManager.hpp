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
    void Init() override;
    Scene* LoadScene(const std::string& file_path);
    void RemoveScene(const std::string& scene_id);
    std::vector<Scene*> GetScenes() const;
    void Update() override;
    
    SceneManager() = default;
    ~SceneManager() = default;

private:
    std::vector<std::string> scene_ids;
    float accumulator = 0.0f;
};
