#include <gtest/gtest.h>
#include "RuntimeFixture.hpp"
#include "engine/components/CircleCollider.hpp"
#include "engine/components/RigidBody.hpp"
#include "engine/components/Transform.hpp"
#include "engine/core/GameObject.hpp"

using CircleColliderTest = RuntimeTest;

TEST_F(CircleColliderTest, AttachingCircleCreatesPhysicsShape) {
    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* circle = new CircleCollider();
    object->AddComponent(circle);

    RigidBody* body = object->GetComponent<RigidBody>();
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(b2Body_GetShapeCount(body->GetBodyId()), 1);
    circle->SetRadius(24.0f);
    EXPECT_EQ(b2Body_GetShapeCount(body->GetBodyId()), 1);
}

TEST_F(CircleColliderTest, RadiusAndCenterReachLiveCircleShape) {
    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* circle = new CircleCollider();
    object->AddComponent(circle);
    auto* body = object->GetComponent<RigidBody>();
    ASSERT_NE(body, nullptr);
    circle->SetRadius(32.0f);
    circle->SetCenter({16.0f, 0.0f});
    b2ShapeId shape{};
    ASSERT_EQ(b2Body_GetShapes(body->GetBodyId(), &shape, 1), 1);
    EXPECT_EQ(b2Shape_GetType(shape), b2_circleShape);
    EXPECT_NEAR(b2Shape_GetCircle(shape).radius, 1.0f, 0.001f);
    EXPECT_NEAR(b2Shape_GetCircle(shape).center.x, 0.5f, 0.001f);
    object->GetTransform()->SetScale({2.0f, 1.0f});
    ASSERT_EQ(b2Body_GetShapes(body->GetBodyId(), &shape, 1), 1);
    EXPECT_NEAR(b2Shape_GetCircle(shape).radius, 2.0f, 0.001f);
}

TEST_F(CircleColliderTest, SensorToggleRebuildsShapeWithoutAddingAnother) {
    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* circle = new CircleCollider();
    object->AddComponent(circle);
    auto* body = object->GetComponent<RigidBody>();
    circle->SetIsSensor(true);
    b2ShapeId shape{};
    ASSERT_EQ(b2Body_GetShapes(body->GetBodyId(), &shape, 1), 1);
    EXPECT_TRUE(b2Shape_IsSensor(shape));
    circle->SetIsSensor(false);
    ASSERT_EQ(b2Body_GetShapes(body->GetBodyId(), &shape, 1), 1);
    EXPECT_FALSE(b2Shape_IsSensor(shape));
}
