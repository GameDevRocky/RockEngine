#include "engine/core/Scene.hpp"
#include <iostream>

Scene::Scene() {}
Scene::Scene(std::string name){this->name = name;}


Scene::~Scene() {}



void Scene::Init() {
    std::cout << "Initializing scene: " << std::endl;
}

void Scene::Update() {
    std::cout << "Updating scene" << std::endl;
}

void Scene::Shutdown() {
    std::cout << "Shutting down scene: " << std::endl;
}
