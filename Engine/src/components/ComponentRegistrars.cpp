#include "engine/components/ComponentRegistrars.hpp"
#include "engine/serialization/SerializableFactory.hpp"
#include "engine/components/Transform.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include <iostream>
#include <filesystem>
#include <windows.h>

namespace fs = std::filesystem;

// Type definitions for DLL exports
using CreateFn = Component* (*)();
using NameFn = const char* (*)();

// Helper to load a DLL and register its component
void RegisterDllComponent(const fs::path& dllPath) {
    HMODULE handle = LoadLibraryA(dllPath.string().c_str());
    if (!handle) {
        std::cerr << "Failed to load DLL: " << dllPath << std::endl;
        return;
    }

    auto create = (CreateFn)GetProcAddress(handle, "CreateComponent");
    auto name = (NameFn)GetProcAddress(handle, "GetTypeName");

    if (!create || !name) {
        std::cerr << "Missing required exports in: " << dllPath << std::endl;
        FreeLibrary(handle);
        return;
    }

    SerializableFactory::RegisterType(name(), [create]() { return create(); });
    std::cout << "Registered DLL component: " << name() << std::endl;
}

void RegisterComponentTypes() {
    SerializableFactory::RegisterType("Transform", []() { return new Transform(); });
    SerializableFactory::RegisterType("SpriteRenderer", []() { return new SpriteRenderer(); });
    fs::path dllFolder = fs::current_path() / "dlls";

    if (fs::exists(dllFolder) && fs::is_directory(dllFolder)) {
        for (auto& entry : fs::directory_iterator(dllFolder)) {
            if (!entry.is_regular_file()) continue;

            fs::path path = entry.path();
            if (path.extension() == ".dll") {
                RegisterDllComponent(path);
            }
        }
    }
}
