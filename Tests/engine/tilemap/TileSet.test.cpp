#include <gtest/gtest.h>
#include <utility>

#include "engine/tilemap/TileSet.hpp"

TEST(TileSetTest, AnyNeighborMatchesBothCornersWhileExactRuleWins) {
    TileSet tileSet;
    Tile tile;
    tile.defaultSpriteId = "default";
    NeighborPattern edge{TileId{0}, TileId{0}, TileId{0}, std::nullopt,
                         TileId{1}, TileId{1}, TileId{1}, TileId{0}};
    NeighborPattern corner = edge;
    corner[3] = TileId{1};
    tile.autoTileRules = {{edge, "edge"}, {corner, "corner"}};
    ASSERT_TRUE(tileSet.SetTile(1, tile));

    NeighborMask neighbors{0, 0, 0, 0, 1, 1, 1, 0};
    EXPECT_EQ(tileSet.ResolveSprite(1, neighbors), "edge");
    neighbors[3] = 1;
    EXPECT_EQ(tileSet.ResolveSprite(1, neighbors), "corner");
    neighbors[3] = 2;
    EXPECT_EQ(tileSet.ResolveSprite(1, neighbors), "edge");
    neighbors[0] = 1;
    EXPECT_EQ(tileSet.ResolveSprite(1, neighbors), "default");
}

TEST(TileSetTest, AnyNeighborSurvivesSerializationAndLegacyExactRulesRemainNumeric) {
    TileSet tileSet;
    Tile tile;
    NeighborPattern wildcard{};
    wildcard.fill(TileId{0});
    wildcard[3] = std::nullopt;
    NeighborPattern exact{};
    exact.fill(TileId{1});
    tile.autoTileRules = {{wildcard, "wildcard"}, {exact, "exact"}};
    ASSERT_TRUE(tileSet.SetTile(1, tile));

    const YAML::Node saved = tileSet.Serialize();
    TileSet restored;
    restored.Deserialize(saved);
    ASSERT_NE(restored.GetTile(1), nullptr);
    EXPECT_EQ(restored.GetTile(1)->autoTileRules, tile.autoTileRules);
    EXPECT_EQ(restored.ResolveSprite(1, NeighborMask{0, 0, 0, 1, 0, 0, 0, 0}), "wildcard");
    EXPECT_EQ(restored.ResolveSprite(1, NeighborMask{1, 1, 1, 1, 1, 1, 1, 1}), "exact");
}

TEST(TileSetTest, EquallySpecificRulesUseStoredOrderAndRoundTripInThatOrder) {
    TileSet tileSet;
    Tile tile;
    NeighborPattern north{};
    north[0] = TileId{0};
    NeighborPattern east{};
    east[2] = TileId{0};
    tile.autoTileRules = {{east, "east"}, {north, "north"}};
    ASSERT_TRUE(tileSet.SetTile(1, tile));
    const NeighborMask emptyNeighbors{};
    EXPECT_EQ(tileSet.ResolveSprite(1, emptyNeighbors), "east");

    std::swap(tile.autoTileRules[0], tile.autoTileRules[1]);
    ASSERT_TRUE(tileSet.SetTile(1, tile));
    EXPECT_EQ(tileSet.ResolveSprite(1, emptyNeighbors), "north");

    TileSet restored;
    restored.Deserialize(tileSet.Serialize());
    ASSERT_NE(restored.GetTile(1), nullptr);
    EXPECT_EQ(restored.GetTile(1)->autoTileRules, tile.autoTileRules);
    EXPECT_EQ(restored.ResolveSprite(1, emptyNeighbors), "north");
}
