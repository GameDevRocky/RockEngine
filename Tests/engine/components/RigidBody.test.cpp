#include <gtest/gtest.h>
#include <memory>
#include "RuntimeFixture.hpp"
#include "engine/components/RigidBody.hpp"
#include "engine/components/Transform.hpp"
#include "engine/core/GameObject.hpp"

using RigidBodyTest = RuntimeTest;

TEST_F(RigidBodyTest, MovingTransformTeleportsBodyAndClearsLinearVelocity) {
    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* body = new RigidBody();
    object->AddComponent(body);
    ASSERT_TRUE(b2Body_IsValid(body->GetBodyId()));

    body->SetLinearVelocity({64.0f, 0.0f});
    EXPECT_NEAR(body->GetLinearVelocity().x, 64.0f, 0.0001f);
    object->GetTransform()->SetPosition({32.0f, 0.0f});

    EXPECT_NEAR(body->GetLinearVelocity().x, 0.0f, 0.0001f);
    EXPECT_NEAR(b2Body_GetPosition(body->GetBodyId()).x, 1.0f, 0.0001f);
}

TEST_F(RigidBodyTest, RotatingTransformKeepsLinearVelocityAndClearsAngularVelocity) {
    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* body = new RigidBody();
    object->AddComponent(body);
    body->SetLinearVelocity({64.0f, 0.0f});
    body->SetAngularVelocity(90.0f);

    object->GetTransform()->SetRotation(45.0f);
    EXPECT_NEAR(body->GetLinearVelocity().x, 64.0f, 0.001f);
    EXPECT_NEAR(body->GetAngularVelocity(), 0.0f, 0.001f);
    EXPECT_NEAR(b2Rot_GetAngle(b2Body_GetRotation(body->GetBodyId())), glm::radians(45.0f), 0.001f);
}

TEST_F(RigidBodyTest, AuthoredPhysicsSettingsReachLiveBodyAndCopyHasNoBody) {
    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* body = new RigidBody();
    object->AddComponent(body);
    body->SetUseGravity(false);
    body->SetBullet(true);
    body->SetLockRotation(true);
    EXPECT_FLOAT_EQ(b2Body_GetGravityScale(body->GetBodyId()), 0.0f);
    EXPECT_TRUE(b2Body_IsBullet(body->GetBodyId()));
    EXPECT_TRUE(b2Body_GetMotionLocks(body->GetBodyId()).angularZ);
    body->SetEnabled(false);
    EXPECT_FALSE(b2Body_IsEnabled(body->GetBodyId()));
    body->SetEnabled(true);
    EXPECT_TRUE(b2Body_IsEnabled(body->GetBodyId()));

    std::unique_ptr<RigidBody> copy(body->Copy());
    ASSERT_NE(copy, nullptr);
    EXPECT_FALSE(b2Body_IsValid(copy->GetBodyId()));
    EXPECT_FALSE(copy->GetUseGravity());
    EXPECT_TRUE(copy->GetBullet());
    EXPECT_TRUE(copy->GetLockRotation());
}
