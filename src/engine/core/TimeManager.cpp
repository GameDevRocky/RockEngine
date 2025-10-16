#include "engine/core/TimeManager.hpp"
#include <iostream>

void TimeManager::Init() {
    lastFrameTime = std::chrono::high_resolution_clock::now();
    deltaTime = 0.0f;
    elapsedTime = 0.0f;
    unscaledTime = 0.0f;
    unscaledDeltaTime = 0.0f;
}

void TimeManager::Update() {
    using namespace std::chrono;
    auto now = high_resolution_clock::now();
    unscaledDeltaTime = duration<float>(now - lastFrameTime).count();
    unscaledTime += unscaledDeltaTime;
    deltaTime = unscaledDeltaTime * timeScale;
    elapsedTime += deltaTime;

    lastFrameTime = now;

    std::cout << "DeltaTime: " << deltaTime
              << " | Unscaled: " << unscaledDeltaTime
              << " | Scale: " << timeScale << std::endl;
}

void TimeManager::Shutdown() {
    
}
