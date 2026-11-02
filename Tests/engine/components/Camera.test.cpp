#include <gtest/gtest.h>
#include <memory>
#include "RuntimeFixture.hpp"
#include "engine/components/Camera.hpp"
#include "engine/components/Transform.hpp"
#include "engine/core/GameObject.hpp"

using CameraTest = RuntimeTest;

TEST_F(CameraTest, MainCameraChoosesHighestPriorityEnabledCamera) {
    auto* lowObject = new GameObject();
    scene->AddGameObject(lowObject);
    auto* low = new Camera();
    lowObject->AddComponent(low);
    low->SetPriority(1);

    auto* highObject = new GameObject();
    scene->AddGameObject(highObject);
    auto* high = new Camera();
    highObject->AddComponent(high);
    high->SetPriority(5);

    EXPECT_EQ(Camera::GetMain(), high);
    high->SetEnabled(false);
    EXPECT_EQ(Camera::GetMain(), low);
}

TEST_F(CameraTest, InactiveHighPriorityCameraFallsBackToActiveCamera) {
    auto* firstObject = new GameObject();
    scene->AddGameObject(firstObject);
    auto* first = new Camera();
    firstObject->AddComponent(first);
    auto* secondObject = new GameObject();
    scene->AddGameObject(secondObject);
    auto* second = new Camera();
    secondObject->AddComponent(second);
    second->SetPriority(10);
    EXPECT_EQ(Camera::GetMain(), second);
    secondObject->SetActive(false);
    EXPECT_EQ(Camera::GetMain(), first);
    first->SetEnabled(false);
    EXPECT_EQ(Camera::GetMain(), nullptr);
}

TEST_F(CameraTest, ApplyToTransfersPoseAndAuthoredViewSettings) {
    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* camera = new Camera();
    object->AddComponent(camera);
    object->GetTransform()->SetPosition({12.0f, -8.0f});
    object->GetTransform()->SetRotation(30.0f);
    camera->SetOrthoSize(240.0f);
    camera->SetTargetAspect(4.0f / 3.0f);
    camera->SetViewportRect({0.1f, 0.2f, 0.5f, 0.6f});
    camera->SetCullingLayers({"Actors", "Terrain"});

    RenderCamera renderCamera;
    camera->ApplyTo(renderCamera);
    EXPECT_EQ(renderCamera.GetPosition(), glm::vec2(12.0f, -8.0f));
    EXPECT_NEAR(renderCamera.GetRotation(), 30.0f, 0.001f);
    EXPECT_FLOAT_EQ(renderCamera.GetOrthoSize(), 240.0f);
    EXPECT_FLOAT_EQ(renderCamera.GetTargetAspect(), 4.0f / 3.0f);
    EXPECT_EQ(renderCamera.GetViewportRect(), glm::vec4(0.1f, 0.2f, 0.5f, 0.6f));
    EXPECT_EQ(renderCamera.GetCullingLayers().count("Actors"), 1u);
    EXPECT_EQ(renderCamera.GetCullingLayers().count("Terrain"), 1u);
}

TEST_F(CameraTest, CopyAndSerializationPreserveCullingAndTarget) {
    Camera camera;
    camera.SetCullingLayers({"Background", "Actors"});
    camera.SetTargetTextureID("render-target");
    camera.SetPriority(7);
    std::unique_ptr<Camera> copy(camera.Copy());
    Camera restored;
    restored.Deserialize(camera.Serialize());
    for (const Camera* result : {copy.get(), &restored}) {
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(result->GetCullingLayers(), camera.GetCullingLayers());
        EXPECT_EQ(result->GetTargetTextureID(), "render-target");
        EXPECT_EQ(result->GetPriority(), 7);
    }
}
