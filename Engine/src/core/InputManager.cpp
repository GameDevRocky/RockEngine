#include "engine/core/InputManager.hpp"
#include <iostream>

void InputManager::Init() {
    std::cout << "InputManager initialized.\n";
    mouse_pos = {0, 0};
}

void InputManager::Update(){
    
}

void InputManager::Shutdown() {
    std::cout << "InputManager shutting down.\n";
}
