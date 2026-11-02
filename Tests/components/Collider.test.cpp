#include <gtest/gtest.h>
#include "RuntimeFixture.hpp"
#include "engine/components/Collider.hpp"
#include "engine/components/RigidBody.hpp"
#include "engine/core/GameObject.hpp"

using ColliderTest = RuntimeTest;

TEST_F(ColliderTest, AddingColliderCreatesKinematicRigidBody) {
    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* collider = new Collider();
    object->AddComponent(collider);

    RigidBody* body = object->GetComponent<RigidBody>();
    ASSERT_NE(body, nullptr);
    EXPECT_TRUE(b2Body_IsValid(body->GetBodyId()));
    EXPECT_EQ(body->GetBodyType(), b2_kinematicBody);
}

TEST_F(ColliderTest, AddingColliderReusesExistingDynamicRigidBody) {
    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* body = new RigidBody();
    object->AddComponent(body);
    auto* collider = new Collider();
    object->AddComponent(collider);
    EXPECT_EQ(object->GetComponent<RigidBody>(), body);
    EXPECT_EQ(body->GetBodyType(), b2_dynamicBody);
    EXPECT_EQ(b2Body_GetShapeCount(body->GetBodyId()), 0);
}

TEST_F(ColliderTest, SharedPropertiesSurviveSerializationAndTriggerOnlyOnChange) {
    Collider collider;
    int centerChanges = 0;
    collider.Subscribe([&centerChanges]() { ++centerChanges; return true; }, Collider::CENTER_CHANGED_EVENT);
    collider.SetCenter({3.0f, 4.0f});
    collider.SetCenter({3.0f, 4.0f});
    collider.SetDensity(2.0f);
    collider.SetFriction(0.4f);
    collider.SetBounciness(0.2f);
    collider.SetIsSensor(true);
    EXPECT_EQ(centerChanges, 1);

    Collider restored;
    restored.Deserialize(collider.Serialize());
    EXPECT_EQ(restored.GetCenter(), glm::vec2(3.0f, 4.0f));
    EXPECT_FLOAT_EQ(restored.GetDensity(), 2.0f);
    EXPECT_FLOAT_EQ(restored.GetFriction(), 0.4f);
    EXPECT_FLOAT_EQ(restored.GetBounciness(), 0.2f);
    EXPECT_TRUE(restored.GetIsSensor());
}
