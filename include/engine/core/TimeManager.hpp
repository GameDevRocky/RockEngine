#pragma once
#include <chrono>
#include "engine/core/System.hpp"
class TimeManager : public System {
public:
    static TimeManager& Get() {
        static TimeManager instance;
        return instance;
    }

    void Init() override;
    void Update() override;
    void Shutdown() override;

    float DeltaTime() const { return deltaTime; }
    float FixedDeltaTime() const { return fixedDeltaTime; }
    float UnscaledTime() const { return unscaledTime; }
    float UnscaledDeltaTime() const { return unscaledDeltaTime; }
    float UnscaledFixedDeltaTime() const { return unscaledFixedDeltaTime; }
    float TimeScale() const { return timeScale; }

    void SetTimeScale(float scale) { timeScale = scale; }

private:
    TimeManager() = default;

    std::chrono::high_resolution_clock::time_point lastFrameTime;

    float deltaTime = 0.0f;
    float fixedDeltaTime = 1.0f / 60.0f; 
    float elapsedTime = 0.0f;
    float unscaledTime = 0.0f;
    float unscaledDeltaTime = 0.0f;
    float unscaledFixedDeltaTime = 1.0f / 60.0f;
    float timeScale = 1.0f;
};
