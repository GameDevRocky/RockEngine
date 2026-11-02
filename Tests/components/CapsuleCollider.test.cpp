#include <gtest/gtest.h>
#include "RuntimeFixture.hpp"
#include "engine/components/CapsuleCollider.hpp"
#include "engine/components/RigidBody.hpp"
#include "engine/core/GameObject.hpp"

using CapsuleColliderTest = RuntimeTest;

TEST_F(CapsuleColliderTest, AttachingCapsuleCreatesPhysicsShape) {
    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* capsule = new CapsuleCollider();
    object->AddComponent(capsule);

    RigidBody* body = object->GetComponent<RigidBody>();
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(b2Body_GetShapeCount(body->GetBodyId()), 1);
    capsule->SetHeight(48.0f);
    EXPECT_EQ(b2Body_GetShapeCount(body->GetBodyId()), 1);
}

TEST_F(CapsuleColliderTest, RadiusHeightAndCenterReachLiveCapsule) {
    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* capsule = new CapsuleCollider();
    object->AddComponent(capsule);
    auto* body = object->GetComponent<RigidBody>();
    ASSERT_NE(body, nullptr);
    capsule->SetRadius(32.0f);
    capsule->SetHeight(64.0f);
    capsule->SetCenter({16.0f, 0.0f});
    b2ShapeId shape{};
    ASSERT_EQ(b2Body_GetShapes(body->GetBodyId(), &shape, 1), 1);
    EXPECT_EQ(b2Shape_GetType(shape), b2_capsuleShape);
    const b2Capsule geometry = b2Shape_GetCapsule(shape);
    EXPECT_NEAR(geometry.radius, 0.5f, 0.001f);
    EXPECT_NEAR(geometry.center1.x, 0.5f, 0.001f);
    EXPECT_NEAR(geometry.center1.y, 1.0f, 0.001f);
    EXPECT_NEAR(geometry.center2.y, -1.0f, 0.001f);
}
