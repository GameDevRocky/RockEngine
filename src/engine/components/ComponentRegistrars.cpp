#include "engine/components/ComponentRegistrars.hpp"
#include "engine/serialization/SerializableFactory.hpp"
#include "engine/components/Transform.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include <iostream>

void RegisterComponentTypes() {
    SerializableFactory::RegisterType("Transform", []() { return new Transform(); });
    SerializableFactory::RegisterType("SpriteRenderer", []() { return new SpriteRenderer(); });
    std::cout << "Components registered" << std::endl;
}
