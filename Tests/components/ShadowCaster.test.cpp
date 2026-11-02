#include <algorithm>
#include <limits>
#include <vector>
#include <gtest/gtest.h>
#include "RuntimeFixture.hpp"
#include "engine/components/ShadowCaster.hpp"
#include "engine/components/Transform.hpp"
#include "engine/core/GameObject.hpp"

using ShadowCasterTest = RuntimeTest;

TEST_F(ShadowCasterTest, BoxOutlineFollowsObjectPosition) {
    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* caster = new ShadowCaster();
    object->AddComponent(caster);
    caster->SetShape(ShadowCaster::Shape::Box);
    caster->SetSize({4.0f, 2.0f});
    object->GetTransform()->SetPosition({10.0f, 20.0f});

    std::vector<glm::vec2> outline;
    caster->BuildOutline(outline);
    ASSERT_EQ(outline.size(), 4u);
    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    for (const auto& point : outline) {
        minX = std::min(minX, point.x);
        maxX = std::max(maxX, point.x);
        minY = std::min(minY, point.y);
        maxY = std::max(maxY, point.y);
    }
    EXPECT_NEAR(minX, 8.0f, 0.0001f);
    EXPECT_NEAR(maxX, 12.0f, 0.0001f);
    EXPECT_NEAR(minY, 19.0f, 0.0001f);
    EXPECT_NEAR(maxY, 21.0f, 0.0001f);
}

TEST_F(ShadowCasterTest, CircleProducesClosedWorldSpaceSegments) {
    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* caster = new ShadowCaster();
    object->AddComponent(caster);
    caster->SetShape(ShadowCaster::Shape::Circle);
    caster->SetRadius(4.0f);
    caster->SetCircleSegments(8);
    object->GetTransform()->SetPosition({10.0f, 20.0f});

    std::vector<glm::vec2> outline;
    caster->BuildOutline(outline);
    ASSERT_EQ(outline.size(), 8u);
    std::vector<glm::vec2> segments;
    caster->AppendSegments(segments);
    ASSERT_EQ(segments.size(), 16u);
    for (std::size_t i = 0; i < outline.size(); ++i) {
        EXPECT_EQ(segments[2 * i], outline[i]);
        EXPECT_EQ(segments[2 * i + 1], outline[(i + 1) % outline.size()]);
    }
}

TEST_F(ShadowCasterTest, InvalidSettingsClampAndUnresolvableShapeProducesNoGeometry) {
    ShadowCaster caster;
    caster.SetSize({-2.0f, 4.0f});
    caster.SetRadius(-1.0f);
    caster.SetCircleSegments(1);
    caster.SetAlphaThreshold(2.0f);
    EXPECT_EQ(caster.GetSize(), glm::vec2(0.0f, 4.0f));
    EXPECT_FLOAT_EQ(caster.GetRadius(), 0.0f);
    EXPECT_EQ(caster.GetCircleSegments(), 3);
    EXPECT_FLOAT_EQ(caster.GetAlphaThreshold(), 1.0f);

    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* attached = new ShadowCaster();
    object->AddComponent(attached);
    attached->SetShape(ShadowCaster::Shape::FromCollider);
    std::vector<glm::vec2> segments;
    attached->AppendSegments(segments);
    EXPECT_TRUE(segments.empty());
}
