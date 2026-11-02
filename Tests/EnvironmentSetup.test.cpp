#include <gtest/gtest.h>
#include "Engine.hpp"

class EngineTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        Engine::Get()->SetAppMode(AppMode::Player);
        Engine::Get()->Init();
        Engine::Get()->PostInit();
    }

    void TearDown() override {
        Engine::Get()->Shutdown();
    }
};

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new EngineTestEnvironment);
    return RUN_ALL_TESTS();
}
