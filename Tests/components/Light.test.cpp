#include <gtest/gtest.h>
#include <memory>
#include "RuntimeFixture.hpp"
#include "engine/components/Light.hpp"
#include "engine/components/Transform.hpp"
#include "engine/core/GameObject.hpp"

using LightTest = RuntimeTest;

TEST_F(LightTest, DirectionFollowsOwningTransform) {
    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* light = new Light();
    object->AddComponent(light);
    object->GetTransform()->SetRotation(90.0f);

    const glm::vec2 direction = light->GetWorldDirection();
    EXPECT_NEAR(direction.x, 0.0f, 0.0001f);
    EXPECT_NEAR(direction.y, 1.0f, 0.0001f);
}

TEST_F(LightTest, ConeAnglesRemainOrdered) {
    Light light;
    light.SetOuterAngle(20.0f);
    light.SetInnerAngle(50.0f);
    EXPECT_FLOAT_EQ(light.GetInnerAngle(), 50.0f);
    EXPECT_FLOAT_EQ(light.GetOuterAngle(), 50.0f);
}

TEST_F(LightTest, LimitsKeepLightParametersInRenderableRanges) {
    Light light;
    light.SetIntensity(-2.0f);
    light.SetRange(0.0f);
    light.SetInnerRadius(2.0f);
    light.SetFalloff(-1.0f);
    light.SetNormalInfluence(-1.0f);
    light.SetShadowStrength(2.0f);
    EXPECT_FLOAT_EQ(light.GetIntensity(), 0.0f);
    EXPECT_FLOAT_EQ(light.GetRange(), 0.001f);
    EXPECT_FLOAT_EQ(light.GetInnerRadius(), 0.99f);
    EXPECT_FLOAT_EQ(light.GetFalloff(), 0.01f);
    EXPECT_FLOAT_EQ(light.GetNormalInfluence(), 0.0f);
    EXPECT_FLOAT_EQ(light.GetShadowStrength(), 1.0f);
    light.SetInnerAngle(200.0f);
    EXPECT_FLOAT_EQ(light.GetInnerAngle(), 180.0f);
    EXPECT_FLOAT_EQ(light.GetOuterAngle(), 180.0f);
    light.SetOuterAngle(-5.0f);
    EXPECT_FLOAT_EQ(light.GetInnerAngle(), 0.0f);
    EXPECT_FLOAT_EQ(light.GetOuterAngle(), 0.0f);
}

TEST_F(LightTest, CopyAndSerializationKeepShadowSettings) {
    Light light;
    light.SetType(Light::LightType::Spot);
    light.SetCastShadows(true);
    light.SetShadowStrength(0.4f);
    light.SetRange(128.0f);
    std::unique_ptr<Light> copy(light.Copy());
    Light restored;
    restored.Deserialize(light.Serialize());
    for (const Light* result : {copy.get(), &restored}) {
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(result->GetType(), Light::LightType::Spot);
        EXPECT_TRUE(result->GetCastShadows());
        EXPECT_FLOAT_EQ(result->GetShadowStrength(), 0.4f);
        EXPECT_FLOAT_EQ(result->GetRange(), 128.0f);
    }
}
