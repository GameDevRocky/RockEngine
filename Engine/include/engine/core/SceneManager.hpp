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

    // Asynchronous: submits a job and returns immediately. The YAML parse runs
    // on a worker thread; the object-graph build necessarily stays on the main
    // thread (Registry, Observable and GenerateUUID are all unsynchronized), so
    // it runs from the job's main step a frame or two later. Callers must not
    // assume the scene exists on return -- subscribe to LOADED_SCENE_EVENT,
    // which still fires exactly once at the end of the build as before.
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
    // Asynchronous, like LoadScene: submits a job so the unload is reported by
    // the loading overlay. The teardown itself is unchanged and still runs in
    // one shot on the main thread -- there is nothing in it that can be threaded.
    void RemoveScene(const std::string& scene_id);
    // The synchronous teardown. Public only because the job's main step calls it
    // on a re-resolved SceneManager; prefer RemoveScene.
    void UnloadSceneNow(const std::string& scene_id);

    std::vector<Scene*> GetScenes() const;

    SceneManager() = default;
    ~SceneManager() = default;

    SceneManager* Copy() override;
    SceneManager* Copy(Container* container) override;

private:
    // The main-thread half of LoadScene: everything from `new Scene` to the
    // LOADED_SCENE_EVENT notify. Split out so the job's main step can call it
    // with an already-parsed document.
    void InstallScene(const YAML::Node& root, const std::string& finalPath);

    std::vector<std::string> scene_ids;
    float accumulator = 0.0f;
};
