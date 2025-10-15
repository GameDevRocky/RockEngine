#pragma once
#include "engine/core/System.hpp"

class InputManager : public System {
public:
    static InputManager& Get() {
        static InputManager instance;
        return instance;
    }

    void Init() override;
    void Update() override;
    void Shutdown() override;

private:
    InputManager() = default;
    ~InputManager() override = default;
};
