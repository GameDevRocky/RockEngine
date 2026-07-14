#pragma once
#include <vector>
#include <algorithm>
#include "engine/core/Scene.hpp"
#include "engine/core/System.hpp"

class Registry;
class TimeManager;
class PhysicsSystem;

class SceneManager : public System {
public:
    static inline const Event LOADED_SCENE_EVENT = SceneManager::CreateEvent();
    static inline const Event REMOVED_SCENE_EVENT = SceneManager::CreateEvent();

    void Init() override;
    void PostInit() override;
    void Awake() override;
    void Start() override;
    void Update() override;
    void Shutdown() override;

    void LoadScene(const std::string& file_path);
    void SaveScene(const std::string& scene_id);
    void RemoveScene(const std::string& scene_id);

    std::vector<Scene*> GetScenes() const;

    SceneManager() = default;
    ~SceneManager() = default;

    SceneManager* Copy() override;
    SceneManager* Copy(Container* container) override;

private:
    std::vector<std::string> scene_ids;
    float accumulator = 0.0f;
};
