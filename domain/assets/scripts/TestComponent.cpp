#include "engine/components/Component.hpp"
#include <iostream>

class TestComponent : public Component {
public:
    // Required by your interface
    std::string GetTypeName() const override { 
        return "TestComponent"; 
    }

    void Start() override {
        std::cout << "TestComponent has started on GameObject: " 
                  << GetGameObject()->GetName() << std::endl;
    }

    void Update() override {
        // This will run every frame when hot-reloaded
        static float timer = 0;
        timer += 0.01f; 
        
        if(timer > 1.0f) {
            std::cout << "TestComponent is ticking..." << std::endl;
            timer = 0;
        }
    }

    void OnDestroy() override {
        std::cout << "TestComponent destroyed!" << std::endl;
    }
};

// 2. The Bridge (The "Factory" Function)
// extern "C" prevents C++ name mangling so GetProcAddress can find "CreateComponent"
extern "C" __declspec(dllexport) Component* CreateComponent() {
    return new TestComponent();
}

// 3. Optional: A cleanup function 
// (Recommended: The DLL that allocates the memory should delete it)
extern "C" __declspec(dllexport) void DestroyComponent(Component* p) {
    delete p;
}