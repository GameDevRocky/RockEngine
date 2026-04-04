#pragma once
#include <chrono>
#include "engine/core/System.hpp"

class Container;

class TimeManager : public System {
    
public:
    TimeManager() = default;

    void Init() override;
    void Update() override;
    void Shutdown() override;

    TimeManager* Copy() override;
    TimeManager* Copy(Container* container) override;

    float DeltaTime() const { return deltaTime; }
    float FixedDeltaTime() const { return fixedDeltaTime; }
    float UnscaledTime() const { return unscaledTime; }
    float UnscaledDeltaTime() const { return unscaledDeltaTime; }
    float ElapsedTime() const { return elapsedTime; }
    float UnscaledFixedDeltaTime() const { return unscaledFixedDeltaTime; }
    float TimeScale() const { return timeScale; }

    void SetTimeScale(float scale) { timeScale = scale; }

    float GetFPS() const { return currentFps; }
    
    private:
    
    std::chrono::high_resolution_clock::time_point lastFrameTime;
    
    float currentFps = 0.0f;
    float deltaTime = 0.0f;
    float fixedDeltaTime = 1.0f / 60.0f; 
    float elapsedTime = 0.0f;
    float unscaledTime = 0.0f;
    float unscaledDeltaTime = 0.0f;
    float unscaledFixedDeltaTime = 1.0f / 60.0f;
    float timeScale = 1.0f;
};
