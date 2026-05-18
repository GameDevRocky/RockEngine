#include "engine/core/InputManager.hpp"
#include <iostream>

void InputManager::Init() {
    mouse_pos = {0, 0};
    std::cout << "InputManager Initialized" << std::endl;
}

void InputManager::Update(){
    m_keysJustPressed = m_keysJustPressedQueue;
    m_keysJustPressedQueue.clear();

    m_mouseButtonsJustPressed = m_mouseButtonsJustPressedQueue;
    m_mouseButtonsJustPressedQueue.clear();
}

void InputManager::Shutdown() {
    std::cout << "InputManager shutting down" << std::endl;
}

InputManager* InputManager::Copy(){
    InputManager* input = new InputManager();
    input->mouse_pos = mouse_pos;
    input->m_keyStates = m_keyStates;
    return input;
}

InputManager* InputManager::Copy(Container* container){
    InputManager* copy = this->Copy();
    copy->Attach(container);
    return copy;
}