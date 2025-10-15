#include "engine/core/InputManager.hpp"
#include <iostream>

void InputManager::Init() {
    std::cout << "InputManager initialized.\n";
}

void InputManager::Update(){
    std::cout << "InputManager updating...\n";
}

void InputManager::Shutdown() {
    std::cout << "InputManager shutting down.\n";
}
