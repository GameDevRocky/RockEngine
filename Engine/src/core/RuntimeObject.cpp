#include "engine/core/RuntimeObject.hpp"
#include "engine/core/Container.hpp"
#include "engine/core/PhysicsSystem.hpp"
#include "engine/core/TimeManager.hpp"
#include "engine/core/SceneManager.hpp"
#include "engine/core/InputManager.hpp"
#include "engine/serialization/Registry.hpp"


void RuntimeObject::Attach(Container* container){
    this->container = container;
    this->registry = container->FindSystem<Registry>();
    this->timeManager = container->FindSystem<TimeManager>();
    this->physicsSystem = container->FindSystem<PhysicsSystem>();
    this->inputManager = container->FindSystem<InputManager>();
    this->sceneManager = container->FindSystem<SceneManager>();

}