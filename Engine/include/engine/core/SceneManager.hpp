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

    // Advances the simulation exactly one fixed timestep + one variable update,
    // independent of the accumulator. Used by the editor's Step button while paused.
    void StepFrame();

    void LoadScene(const std::string& file_path);
    // allowRuntimeSave bypasses the Editor-only guard: intended for scripted,
    // explicit "promote runtime state to disk" tools (e.g. an in-game level
    // painter) that want the live Runtime-container scene written back over
    // the file it was loaded from. Defaults to false so the editor's own Save
    // Scene action keeps refusing to overwrite the authored scene with
    // throwaway play-mode state.
    // Returns true once the scene was actually serialized and written to disk;
    // false for every early-out (wrong mode, unknown scene, no path, I/O failure).
    bool SaveScene(const std::string& scene_id, bool allowRuntimeSave = false);
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
