#pragma once

#include <filesystem>

#include <gtest/gtest.h>

#include "Engine.hpp"
#include "engine/core/SceneManager.hpp"
#include "engine/jobs/JobSystem.hpp"
#include "engine/serialization/Registry.hpp"

class EditorSceneTest : public ::testing::Test {
protected:
    Container* editorContainer = nullptr;
    SceneManager* sceneManager = nullptr;
    Registry* registry = nullptr;
    Scene* scene = nullptr;

    void SetUp() override {
        editorContainer = Engine::Get()->GetEditorContainer();
        ASSERT_NE(editorContainer, nullptr);
        ASSERT_EQ(editorContainer, Engine::Get()->GetActiveContainer());
        sceneManager = editorContainer->FindSystem<SceneManager>();
        registry = editorContainer->FindSystem<Registry>();
        ASSERT_NE(sceneManager, nullptr);
        ASSERT_NE(registry, nullptr);

        const auto path = std::filesystem::path(PROJECT_ROOT) / "Tests" / "mocks" / "mock.scene";
        ASSERT_TRUE(std::filesystem::exists(path)) << path.string();
        sceneManager->LoadScene(path.string());
        while (!scene) {
            JobSystem::Get().Pump();
            scene = registry->Find<Scene>("mock-scene");
        }
    }

    void TearDown() override {
        if (scene && sceneManager) sceneManager->UnloadSceneNow(scene->GetID());
        if (registry) registry->FlushPendingShutdowns();
    }
};
