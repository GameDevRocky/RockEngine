#pragma once

#include <filesystem>

#include <gtest/gtest.h>
#include "Engine.hpp"
#include "engine/jobs/JobSystem.hpp"
#include "engine/serialization/Registry.hpp"

class RuntimeTest : public ::testing::Test {
protected:
    Container* runtimeContainer = nullptr;
    SceneManager* sceneManager = nullptr;
    Registry* registry = nullptr;
    Scene* scene = nullptr;

    void SetUp() override {
        Engine::Get()->EnterPlayMode();
        runtimeContainer = Engine::Get()->GetActiveContainer();
        ASSERT_NE(runtimeContainer, nullptr);
        ASSERT_EQ(runtimeContainer, Engine::Get()->GetRuntimeContainer());
        sceneManager = runtimeContainer->FindSystem<SceneManager>();
        ASSERT_NE(sceneManager, nullptr);
        registry = runtimeContainer->FindSystem<Registry>();
        ASSERT_NE(registry, nullptr);

        const auto scene_path = std::filesystem::path(PROJECT_ROOT) / "Tests" / "mocks" / "mock.scene";
        ASSERT_TRUE(std::filesystem::exists(scene_path)) << scene_path.string();
        sceneManager->LoadScene(scene_path.string());

        while (!scene) {
            JobSystem::Get().Pump();
            scene = registry->Find<Scene>("mock-scene");
        }
    }

    void TearDown() override {
        if (Engine::Get()->GetRuntimeContainer()) {
            Engine::Get()->ExitPlayMode();
        }
    }
};
