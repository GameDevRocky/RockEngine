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

TEST(TileSetTest, CollisionTypeRoundTripsAndLegacyOrUnknownValuesDefaultToNone) {
    TileSet tileSet;
    Tile solid;
    solid.collisionType = TileCollisionType::Grid;
    ASSERT_TRUE(tileSet.SetTile(1, solid));

    const YAML::Node saved = tileSet.Serialize();
    ASSERT_EQ(saved["tiles"][0]["collisionType"].as<std::string>(), "Grid");
    TileSet restored;
    restored.Deserialize(saved);
    ASSERT_NE(restored.GetTile(1), nullptr);
    EXPECT_EQ(restored.GetTile(1)->collisionType, TileCollisionType::Grid);

    YAML::Node legacy = saved;
    legacy["tiles"][0].remove("collisionType");
    restored.Deserialize(legacy);
    ASSERT_NE(restored.GetTile(1), nullptr);
    EXPECT_EQ(restored.GetTile(1)->collisionType, TileCollisionType::None);

    legacy["tiles"][0]["collisionType"] = "FutureShape";
    restored.Deserialize(legacy);
    ASSERT_NE(restored.GetTile(1), nullptr);
    EXPECT_EQ(restored.GetTile(1)->collisionType, TileCollisionType::None);

    legacy["tiles"][0]["collisionType"] = "FullCell";
    restored.Deserialize(legacy);
    ASSERT_NE(restored.GetTile(1), nullptr);
    EXPECT_EQ(restored.GetTile(1)->collisionType, TileCollisionType::Grid);
}

TEST(TileSetTest, RandomAndAnimationOutputsResolveAndRoundTrip) {
    TileSet tileSet;
    Tile tile;
    AutoTileRule rule;
    rule.outputType = TileOutputType::Random;
    rule.collisionType = TileCollisionType::Sprite;
    rule.spriteIds = {"random-a", "random-b", "random-c"};
    tile.autoTileRules.push_back(rule);
    ASSERT_TRUE(tileSet.SetTile(7, tile));

    const ResolvedTile first = tileSet.ResolveTile(7, {}, {4, -2}, 0.0f);
    const ResolvedTile repeated = tileSet.ResolveTile(7, {}, {4, -2}, 100.0f);
    EXPECT_EQ(first.spriteId, repeated.spriteId);
    EXPECT_NE(std::find(rule.spriteIds.begin(), rule.spriteIds.end(), first.spriteId), rule.spriteIds.end());
    EXPECT_EQ(first.outputType, TileOutputType::Random);
    EXPECT_EQ(first.collisionType, TileCollisionType::Sprite);

    tile.autoTileRules[0].outputType = TileOutputType::Animation;
    tile.autoTileRules[0].spriteIds = {"frame-0", "frame-1", "frame-2"};
    tile.autoTileRules[0].minAnimationSpeed = 2.0f;
    tile.autoTileRules[0].maxAnimationSpeed = 2.0f;
    ASSERT_TRUE(tileSet.SetTile(7, tile));
    EXPECT_EQ(tileSet.ResolveTile(7, {}, {4, -2}, 0.0f).spriteId, "frame-0");
    EXPECT_EQ(tileSet.ResolveTile(7, {}, {4, -2}, 0.6f).spriteId, "frame-1");

    TileSet restored;
    restored.Deserialize(tileSet.Serialize());
    const Tile* restoredTile = restored.GetTile(7);
    ASSERT_NE(restoredTile, nullptr);
    ASSERT_EQ(restoredTile->autoTileRules.size(), 1u);
    EXPECT_EQ(restoredTile->autoTileRules[0], tile.autoTileRules[0]);
}

TEST(TileSetTest, CustomPolygonsRoundTripAndResolveFromMatchingRule) {
    TileSet tileSet;
    Tile tile;
    tile.collisionType = TileCollisionType::Custom;
    tile.customPolygon = {{-0.5f, -0.5f}, {0.5f, -0.5f}, {0.25f, 0.5f}};
    AutoTileRule rule;
    rule.collisionType = TileCollisionType::Custom;
    rule.customPolygon = {{-0.5f, -0.5f}, {0.5f, -0.5f}, {0.5f, 0.25f}, {-0.5f, 0.5f}};
    rule.neighbors[0] = TileId{2};
    tile.autoTileRules.push_back(rule);
    ASSERT_TRUE(tileSet.SetTile(1, tile));

    NeighborMask unmatched{};
    const ResolvedTile fallback = tileSet.ResolveTile(1, unmatched);
    ASSERT_NE(fallback.customPolygon, nullptr);
    EXPECT_EQ(*fallback.customPolygon, tile.customPolygon);
    unmatched[0] = 2;
    const ResolvedTile matched = tileSet.ResolveTile(1, unmatched);
    ASSERT_NE(matched.customPolygon, nullptr);
    EXPECT_EQ(*matched.customPolygon, rule.customPolygon);

    TileSet restored;
    restored.Deserialize(tileSet.Serialize());
    ASSERT_NE(restored.GetTile(1), nullptr);
    EXPECT_EQ(restored.GetTile(1)->customPolygon, tile.customPolygon);
    ASSERT_EQ(restored.GetTile(1)->autoTileRules.size(), 1u);
    EXPECT_EQ(restored.GetTile(1)->autoTileRules[0].customPolygon, rule.customPolygon);
}

TEST(TileSetTest, CustomPolygonsRequireThreeToEightConvexPoints) {
    TileSet tileSet;
    Tile tile;
    tile.collisionType = TileCollisionType::Custom;
    tile.customPolygon = {{0.0f, 0.0f}, {1.0f, 0.0f}};
    EXPECT_FALSE(tileSet.SetTile(1, tile));
    tile.customPolygon = {{0.0f, 0.0f}, {1.0f, 0.0f}, {0.2f, 0.2f}, {1.0f, 1.0f}};
    EXPECT_FALSE(tileSet.SetTile(1, tile));
    tile.customPolygon = {{0.0f, 1.0f}, {0.59f, -0.81f}, {-0.95f, 0.31f},
                          {0.95f, 0.31f}, {-0.59f, -0.81f}};
    EXPECT_FALSE(tileSet.SetTile(1, tile));
    tile.customPolygon = {{-0.5f, -0.5f}, {0.6f, -0.5f}, {0.5f, 0.5f}};
    EXPECT_FALSE(tileSet.SetTile(1, tile));
    tile.collisionType = TileCollisionType::None;
    EXPECT_FALSE(tileSet.SetTile(1, tile));
    tile.collisionType = TileCollisionType::Custom;
    tile.customPolygon = DefaultTileCollisionPolygon();
    EXPECT_TRUE(tileSet.SetTile(1, tile));

    YAML::Node malformed = tileSet.Serialize();
    malformed["tiles"][0]["customPolygon"] = YAML::Load("[[0, 0], [1, 0]]");
    TileSet restored;
    restored.Deserialize(malformed);
    ASSERT_NE(restored.GetTile(1), nullptr);
    EXPECT_EQ(restored.GetTile(1)->customPolygon, DefaultTileCollisionPolygon());

    malformed["tiles"][0]["customPolygon"] = YAML::Load(
        "[[-2, -2], [2, -2], [2, 2], [-2, 2]]");
    restored.Deserialize(malformed);
    ASSERT_NE(restored.GetTile(1), nullptr);
    EXPECT_EQ(restored.GetTile(1)->customPolygon, DefaultTileCollisionPolygon());
}
