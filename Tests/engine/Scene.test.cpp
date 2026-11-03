#include <gtest/gtest.h>
#include <memory>
#include "engine/core/Scene.hpp"

TEST(SceneTest, CopyDoesNotCarrySourceEventSubscriptions) {
    Scene scene;
    scene.SetPath("test-scene.scene");
    int sourceNotifications = 0;
    scene.Subscribe([&sourceNotifications]() {
        ++sourceNotifications;
        return true;
    }, Scene::SHUTDOWN_EVENT);

    std::unique_ptr<Scene> copy(scene.Copy());
    EXPECT_EQ(copy->GetPath(), scene.GetPath());
    copy->Shutdown();
    EXPECT_EQ(sourceNotifications, 0);

    scene.Shutdown();
    EXPECT_EQ(sourceNotifications, 1);
}
