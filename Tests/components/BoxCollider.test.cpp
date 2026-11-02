#include <gtest/gtest.h>
#include <algorithm>
#include "RuntimeFixture.hpp"
#include "engine/components/BoxCollider.hpp"
#include "engine/components/RigidBody.hpp"
#include "engine/components/Transform.hpp"
#include "engine/core/GameObject.hpp"

using BoxColliderTest = RuntimeTest;

TEST_F(BoxColliderTest, AttachingBoxCreatesAndRebuildsOnePhysicsShape) {
    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* box = new BoxCollider();
    object->AddComponent(box);

    RigidBody* body = object->GetComponent<RigidBody>();
    ASSERT_NE(body, nullptr);
    ASSERT_TRUE(b2Body_IsValid(body->GetBodyId()));
    EXPECT_EQ(b2Body_GetShapeCount(body->GetBodyId()), 1);

    box->SetSize({64.0f, 32.0f});
    EXPECT_EQ(b2Body_GetShapeCount(body->GetBodyId()), 1);
    EXPECT_EQ(box->GetSize(), glm::vec2(64.0f, 32.0f));
}

TEST_F(BoxColliderTest, ResizingAndScalingChangeLivePolygonExtents) {
    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* box = new BoxCollider();
    object->AddComponent(box);
    auto* body = object->GetComponent<RigidBody>();
    ASSERT_NE(body, nullptr);
    box->SetSize({64.0f, 32.0f});
    b2ShapeId shape{};
    ASSERT_EQ(b2Body_GetShapes(body->GetBodyId(), &shape, 1), 1);
    EXPECT_EQ(b2Shape_GetType(shape), b2_polygonShape);
    const auto widthOf = [](b2ShapeId id) {
        const b2Polygon polygon = b2Shape_GetPolygon(id);
        float minimum = polygon.vertices[0].x;
        float maximum = polygon.vertices[0].x;
        for (int index = 1; index < polygon.count; ++index) {
            minimum = std::min(minimum, polygon.vertices[index].x);
            maximum = std::max(maximum, polygon.vertices[index].x);
        }
        return maximum - minimum;
    };
    EXPECT_NEAR(widthOf(shape), 2.0f, 0.001f);
    object->GetTransform()->SetScale({2.0f, 1.0f});
    ASSERT_EQ(b2Body_GetShapes(body->GetBodyId(), &shape, 1), 1);
    EXPECT_NEAR(widthOf(shape), 4.0f, 0.001f);
}

TEST_F(BoxColliderTest, DisableRemovesMassAndReenableRestoresAuthoredDensity) {
    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* box = new BoxCollider();
    object->AddComponent(box);
    auto* body = object->GetComponent<RigidBody>();
    b2ShapeId shape{};
    ASSERT_EQ(b2Body_GetShapes(body->GetBodyId(), &shape, 1), 1);
    box->SetDensity(2.0f);
    EXPECT_FLOAT_EQ(b2Shape_GetDensity(shape), 2.0f);
    box->SetEnabled(false);
    box->SetDensity(3.0f);
    EXPECT_FLOAT_EQ(b2Shape_GetDensity(shape), 0.0f);
    box->SetEnabled(true);
    EXPECT_FLOAT_EQ(b2Shape_GetDensity(shape), 3.0f);
}
