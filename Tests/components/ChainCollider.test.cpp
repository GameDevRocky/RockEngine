#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include "RuntimeFixture.hpp"
#include "engine/components/ChainCollider.hpp"

using ChainColliderTest = RuntimeTest;

TEST_F(ChainColliderTest, RejectsSelfIntersectingLoop) {
    ChainCollider chain;
    chain.SetPoints({{0.0f, 0.0f}, {32.0f, 0.0f}, {32.0f, 32.0f}, {0.0f, 32.0f}});
    EXPECT_TRUE(chain.HasValidGeometry());

    chain.SetPoints({{0.0f, 0.0f}, {32.0f, 32.0f}, {0.0f, 32.0f}, {32.0f, 0.0f}});
    EXPECT_FALSE(chain.HasValidGeometry());
}

TEST_F(ChainColliderTest, ValidatesMinimumPointsAndDegenerateEdges) {
    ChainCollider chain;
    chain.SetPoints({{0, 0}, {32, 0}, {32, 32}});
    EXPECT_FALSE(chain.HasValidGeometry());
    chain.SetIsLoop(false);
    EXPECT_TRUE(chain.HasValidGeometry());
    chain.SetPoints({{0, 0}});
    EXPECT_FALSE(chain.HasValidGeometry());
    chain.SetPoints({{0, 0}, {0, 0}, {32, 0}});
    EXPECT_FALSE(chain.HasValidGeometry());
    chain.SetPoints({{0, 0}, {std::numeric_limits<float>::infinity(), 0}});
    EXPECT_FALSE(chain.HasValidGeometry());
}

TEST_F(ChainColliderTest, PointEditsRejectInvalidIndicesWithoutChangingGeometry) {
    ChainCollider chain;
    const auto initial = chain.GetPoints();
    EXPECT_FALSE(chain.SetPoint(99, {1, 1}));
    EXPECT_FALSE(chain.InsertPoint(99, {1, 1}));
    EXPECT_FALSE(chain.RemovePoint(99));
    EXPECT_EQ(chain.GetPoints(), initial);
    EXPECT_TRUE(chain.SetPoint(0, {-32, -32}));
    EXPECT_TRUE(chain.InsertPoint(1, {0, -32}));
    EXPECT_TRUE(chain.RemovePoint(1));
    EXPECT_EQ(chain.GetPoints().front(), glm::vec2(-32, -32));
    EXPECT_EQ(chain.GetPoints().size(), initial.size());
}

TEST_F(ChainColliderTest, CopyAndSerializationKeepOutlineSettings) {
    ChainCollider chain;
    chain.SetIsLoop(false);
    chain.SetReverseSide(true);
    chain.SetAlphaThreshold(2.0f);
    chain.SetPoints({{0, 0}, {32, 0}, {32, 32}});
    EXPECT_FLOAT_EQ(chain.GetAlphaThreshold(), 1.0f);

    std::unique_ptr<ChainCollider> copy(chain.Copy());
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->GetPoints(), chain.GetPoints());
    EXPECT_FALSE(copy->GetIsLoop());
    EXPECT_TRUE(copy->GetReverseSide());

    ChainCollider restored;
    restored.Deserialize(chain.Serialize());
    EXPECT_EQ(restored.GetPoints(), chain.GetPoints());
    EXPECT_FALSE(restored.GetIsLoop());
    EXPECT_TRUE(restored.GetReverseSide());
    EXPECT_FLOAT_EQ(restored.GetAlphaThreshold(), 1.0f);
}
