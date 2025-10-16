#pragma once
#include <vector>
#include <iostream>
#include <string>

class Scene {

public:
    Scene();
    Scene(std::string name);
    ~Scene();

    void Init();
    void Update();
    void Shutdown();

protected:
    bool active = true;

private:
    std::string name;
    

};
