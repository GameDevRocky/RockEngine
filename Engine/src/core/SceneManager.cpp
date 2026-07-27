#include "engine/core/SceneManager.hpp"
#include <algorithm>
#include <iostream>
#include "engine/serialization/Registry.hpp"
#include "engine/core/TimeManager.hpp"
#include "engine/core/PhysicsSystem.hpp"
#include "Engine.hpp"
#include "engine/utils/EngineUtils.hpp"
#include <filesystem>
#include <fstream>
#include "engine/debug/Console.hpp"
#include "engine/debug/FrameProfiler.hpp"

using namespace EngineUtils;
void SceneManager::Init(){

    registry = container->FindSystem<Registry>();
    timeManager = container->FindSystem<TimeManager>();
    physicsSystem = container->FindSystem<PhysicsSystem>();
    if (state >= State::Initialized) return ;
    for (auto* scene : GetScenes()){
        scene->Init();
    }
    state = State::Initialized;
    std::cout << "SceneManager Initialized" << std::endl;
}
void SceneManager::PostInit(){

    if (state >= State::PostInitialized) return ;
    for (auto* scene : GetScenes()){
        scene->PostInit();
    }
    state = State::PostInitialized;
    std::cout << "SceneManager Post Initialized" << std::endl;
}
void SceneManager::Awake(){

    if (state >= State::Awakened) return ;
    for (auto* scene : GetScenes()){
        scene->Awake();
    }
    state = State::Awakened;
    std::cout << "SceneManager Awakened" << std::endl;
}
void SceneManager::Start(){

    if (state >= State::Started) return ;
    for (auto* scene : GetScenes()){
        scene->Start();
    }
    state = State::Started;
    std::cout << "SceneManager Started" << std::endl;
}

void SceneManager::Update() {
    // Frozen while paused: return before touching the accumulator so no time
    // debt builds up. Rendering (Qt paintGL) and TimeManager keep running.
    if (container->IsPaused()) return;

    const float fixedDeltaTime = timeManager->FixedDeltaTime();
    const float frameTime = timeManager->DeltaTime();

    accumulator += frameTime;

    // Clamp the time debt (Unity's maximumDeltaTime): a slow frame otherwise
    // queues more substeps, making the next frame slower — a death spiral
    // (observed: 43 substeps in one frame at 200 objects). Under overload the
    // simulation runs briefly in slow-motion instead of compounding.
    constexpr int kMaxSubstepsPerFrame = 4;
    const float maxDebt = kMaxSubstepsPerFrame * fixedDeltaTime;
    if (accumulator > maxDebt) accumulator = maxDebt;

    while (accumulator >= fixedDeltaTime) {
        ROCK_PROFILE_COUNT("fixed substeps", 1);
        {
            ROCK_PROFILE_SCOPE("Scene::FixedUpdate");
            for (auto& scene_id : scene_ids) {
                Scene* scene = registry->Find<Scene>(scene_id);
                if (!scene){
                    std::cout << "Error in Scene Manager, scene is null" << std::endl;
                    continue;
                }
                scene->FixedUpdate();
            }
        }
        {
            ROCK_PROFILE_SCOPE("PhysicsSystem::Step");
            physicsSystem->Step();
        }
        accumulator -= fixedDeltaTime;
    }

    {
        ROCK_PROFILE_SCOPE("Scene::Update");
        for (auto& scene_id : scene_ids) {
            Scene* scene = registry->Find<Scene>(scene_id);
            if (!scene) continue;
            scene->Update();
        }
    }

    {
        ROCK_PROFILE_SCOPE("Scene::LateUpdate");
        for (auto& scene_id : scene_ids) {
            Scene* scene = registry->Find<Scene>(scene_id);
            if (!scene) continue;
            scene->LateUpdate();
        }
    }

    registry->FlushPendingShutdowns();
}

void SceneManager::StepFrame() {
    // Exactly one fixed step (FixedUpdate + physics), then one variable update.
    // The accumulator is intentionally left untouched so resuming stays clean.
    for (auto& scene_id : scene_ids) {
        Scene* scene = registry->Find<Scene>(scene_id);
        if (!scene) continue;
        scene->FixedUpdate();
    }
    physicsSystem->Step();

    for (auto& scene_id : scene_ids) {
        Scene* scene = registry->Find<Scene>(scene_id);
        if (!scene) continue;
        scene->Update();
    }

    for (auto& scene_id : scene_ids) {
        Scene* scene = registry->Find<Scene>(scene_id);
        if (!scene) continue;
        scene->LateUpdate();
    }

    registry->FlushPendingShutdowns();
}


void SceneManager::LoadScene(const std::string& file_path){
    std::string finalPath = file_path;

    std::filesystem::path p(file_path);
    if (!p.is_absolute()) {
        finalPath = GetAssetPath(file_path);
    }

    if (!std::filesystem::exists(finalPath)) {
        return;
    }
    std::cout << "Loading scene from path: " + file_path << std::endl;
    YAML::Node root = YAML::LoadFile(finalPath);
    
    if (root["id"]) {
        std::string id = root["id"].as<std::string>();
        if (std::find(scene_ids.begin(), scene_ids.end(), id) != scene_ids.end()) {
            std::cout << "Scene already loaded: " + id << std::endl;
            return;
        }
    }

    Scene* scene = new Scene();
    scene->Attach(container);

    std::cout << "Deserializing scene from path: " + file_path << std::endl;
    scene->Deserialize(root);
    scene->SetPath(finalPath);
    registry->Register(scene);
    
    std::cout << "Initializing scene from path: " + file_path << std::endl;
    scene->Init();
    scene->PostInit();
    
    if (container->GetMode() == Container::Mode::Runtime){
        std::cout << "Awakening scene (Runtime mode): " + file_path << std::endl;
        scene->Awake();
        scene->Start();
    }

    scene_ids.push_back(scene->GetID());
    std::cout << "Completed loading scene from path: " + file_path << std::endl;
    Notify(LOADED_SCENE_EVENT, scene->GetID());
}


bool SceneManager::SaveScene(const std::string& scene_id, bool allowRuntimeSave) {
    // Saving is only meaningful against the edit-time world by default. In play
    // mode the active container is a throwaway deep copy, so persisting it would
    // write the simulated (moved/spawned) state back over the authored scene.
    // allowRuntimeSave is the explicit, scripted opt-out of that guard (see the
    // header comment) — the editor's own Save Scene menu never sets it.
    if (!allowRuntimeSave && container->GetMode() != Container::Mode::Editor) {
        Console::Warn("Only able to save scene in Editor mode");
        return false;
    }

    Scene* scene = registry->Find<Scene>(scene_id);
    if (!scene) return false;

    const std::string& path = scene->GetPath();
    if (path.empty()) {
        std::cout << "Scene has no path; cannot save: " + scene_id << std::endl;
        return false;
    }

    YAML::Node node = scene->Serialize();
    std::ofstream fout(path);
    if (!fout.is_open()) {
        std::cout << "Failed to open scene file for saving: " + path << std::endl;
        return false;
    }
    fout << node;
    std::cout << "Saved scene to: " + path << std::endl;
    return true;
}

void SceneManager::RemoveScene(const std::string& scene_id) {
    auto* scene = registry->Find<Scene>(scene_id);
    if(!scene) return;
    scene_ids.erase(std::remove(scene_ids.begin(), scene_ids.end(), scene_id), scene_ids.end());
    scene->Shutdown();
    Notify(REMOVED_SCENE_EVENT, scene_id);
}

std::vector<Scene*> SceneManager::GetScenes() const {
    std::vector<Scene*> scenes;
    for (auto& s_id : scene_ids){
        Scene* scene = registry->Find<Scene>(s_id);
        if (scene) scenes.push_back(scene);
    }
    return scenes;
}

void SceneManager::Shutdown(){
   
}

SceneManager* SceneManager::Copy(){
    SceneManager* copy = new SceneManager();
    copy->scene_ids = scene_ids;
    copy->accumulator = 0;
    copy->subscribers = subscribers;
    return copy;
    
}

SceneManager* SceneManager::Copy(Container* container){
    SceneManager* copy = this->Copy();
    copy->Attach(container);
    return copy;
}


