#include <gtest/gtest.h>
#include "RuntimeFixture.hpp"
#include "engine/components/Grid.hpp"
#include "engine/components/Transform.hpp"
#include "engine/core/GameObject.hpp"

using GridTest = RuntimeTest;

TEST_F(GridTest, CellConversionUsesGapAndObjectTransform) {
    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* grid = new Grid();
    object->AddComponent(grid);
    object->GetTransform()->SetPosition({5.0f, 7.0f});
    grid->SetSize({10.0f, 20.0f});
    grid->SetGap({2.0f, 4.0f});

    const glm::vec2 world = grid->CellToWorld({2, 3});
    EXPECT_FLOAT_EQ(world.x, 29.0f);
    EXPECT_FLOAT_EQ(world.y, 79.0f);
    EXPECT_EQ(grid->WorldToCell(world), glm::ivec2(2, 3));
}

TEST_F(GridTest, NegativeCoordinatesFloorToPreviousCell) {
    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* grid = new Grid();
    object->AddComponent(grid);
    grid->SetSize({10.0f, 20.0f});
    grid->SetGap({2.0f, 4.0f});
    EXPECT_EQ(grid->WorldToCell({-0.1f, -24.1f}), glm::ivec2(-1, -2));
    EXPECT_EQ(grid->CellToWorld({-2, 1}), glm::vec2(-24.0f, 24.0f));
}

TEST_F(GridTest, RotatedObjectRoundTripsCellCoordinates) {
    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* grid = new Grid();
    object->AddComponent(grid);
    grid->SetSize({16.0f, 16.0f});
    object->GetTransform()->SetPosition({20.0f, -10.0f});
    object->GetTransform()->SetRotation(90.0f);
    const glm::vec4 insideCell = object->GetTransform()->GetWorldMatrix()
        * glm::vec4(33.0f, 49.0f, 0.0f, 1.0f);
    EXPECT_EQ(grid->WorldToCell({insideCell.x, insideCell.y}), glm::ivec2(2, 3));
}

TEST_F(GridTest, DegenerateStrideAndTransformReturnOriginCell) {
    auto* zeroStrideObject = new GameObject();
    scene->AddGameObject(zeroStrideObject);
    auto* zeroStrideGrid = new Grid();
    zeroStrideObject->AddComponent(zeroStrideGrid);
    zeroStrideGrid->SetSize({0.0f, 10.0f});
    zeroStrideGrid->SetGap({0.0f, 0.0f});
    EXPECT_EQ(zeroStrideGrid->WorldToCell({100.0f, 100.0f}), glm::ivec2(0, 0));

    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* attached = new Grid();
    object->AddComponent(attached);
    object->GetTransform()->SetScale({0.0f, 1.0f});
    EXPECT_EQ(attached->WorldToCell({100.0f, 100.0f}), glm::ivec2(0, 0));
}
