#include <gtest/gtest.h>
#include <memory>

#include "engine/core/GameObject.hpp"
#include "engine/rendering/core/Material.hpp"
#include "utils/InspectorVisitor.hpp"

TEST(InspectorVisitorTest, DefersRowsForObjectsAndAssets) {
    GameObject object;
    InspectorVisitor objectVisitor(true);
    std::unique_ptr<QWidget> objectContent(objectVisitor.GetContent());
    object.Accept(&objectVisitor);
    ASSERT_TRUE(objectVisitor.HasPendingRows());
    EXPECT_EQ(objectVisitor.GetContent()->layout()->count(), 0);
    objectVisitor.BuildNextRow();
    EXPECT_GT(objectVisitor.GetContent()->layout()->count(), 0);

    Material material;
    InspectorVisitor assetVisitor(true);
    std::unique_ptr<QWidget> assetContent(assetVisitor.GetContent());
    material.Accept(&assetVisitor);
    ASSERT_TRUE(assetVisitor.HasPendingRows());
    EXPECT_EQ(assetVisitor.GetContent()->layout()->count(), 0);
    assetVisitor.BuildNextRow();
    EXPECT_GT(assetVisitor.GetContent()->layout()->count(), 0);
}
