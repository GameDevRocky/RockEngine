#include "engine/components/ComponentRegistrars.hpp"
#include "engine/serialization/SerializableFactory.hpp"
#include "engine/components/Transform.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include "engine/components/ScriptComponent.hpp"
#include <iostream>
#include <filesystem>
#include <windows.h>


void RegisterComponentTypes() {
    SerializableFactory::RegisterType("Transform", []() { return new Transform(); });
    SerializableFactory::RegisterType("SpriteRenderer", []() { return new SpriteRenderer(); });
    SerializableFactory::RegisterType("ScriptComponent", []() { return new ScriptComponent(); });
    
}
