#include "engine/core/TimeManager.hpp"
#include <iostream>
#include "engine/debug/Console.hpp"
void TimeManager::Init() {
    lastFrameTime = std::chrono::high_resolution_clock::now();
    deltaTime = 0.0f;
    elapsedTime = 0.0f;
    unscaledTime = 0.0f;
    unscaledDeltaTime = 0.0f;
    std::cout << "TimeManager Initialized" << std::endl;
}


void TimeManager::Update() {
    using namespace std::chrono;
    auto now = high_resolution_clock::now();
    unscaledDeltaTime = duration<float>(now - lastFrameTime).count();
    
    if (unscaledDeltaTime > 0.0f) {
        float instantaneousFps = 1.0f / unscaledDeltaTime;
        currentFps = (currentFps * 0.9f) + (instantaneousFps * 0.1f);
    }

    unscaledTime += unscaledDeltaTime;
    deltaTime = unscaledDeltaTime * timeScale;
    elapsedTime += deltaTime;
    lastFrameTime = now;
}

void TimeManager::Shutdown() {

}

TimeManager* TimeManager::Copy() {
    TimeManager* copy = new TimeManager();
    return copy;
}

TimeManager* TimeManager::Copy(Container* container){
    TimeManager* copy = this->Copy();
    copy->Attach(container);
    return copy;
}

