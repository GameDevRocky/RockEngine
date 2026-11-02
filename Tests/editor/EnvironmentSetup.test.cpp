#include <QApplication>
#include <gtest/gtest.h>

#include "Engine.hpp"

class EditorTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        Engine::Get()->SetAppMode(AppMode::Editor);
        Engine::Get()->Init();
        Engine::Get()->PostInit();
    }

    void TearDown() override {
        Engine::Get()->Shutdown();
    }
};

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    QApplication app(argc, argv);
    ::testing::AddGlobalTestEnvironment(new EditorTestEnvironment);
    return RUN_ALL_TESTS();
}
