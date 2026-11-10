#include <algorithm>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QSpinBox>
#include <QTest>
#include <QToolButton>
#include <gtest/gtest.h>

#include "engine/tilemap/TileSet.hpp"
#include "utils/TileSetInspectorWidget.hpp"

namespace {
bool HasPattern(const Tile& tile, const NeighborPattern& pattern) {
    return std::any_of(tile.autoTileRules.begin(), tile.autoTileRules.end(),
        [&](const AutoTileRule& rule) { return rule.neighbors == pattern; });
}
}

TEST(TileSetInspectorTest, TileListSelectionShowsAttributesAndRules) {
    TileSet tileSet;
    Tile grass;
    grass.name = "Grass";
    NeighborPattern neighbors{TileId{1}, TileId{2}, TileId{3}, TileId{4},
                              TileId{5}, TileId{6}, TileId{7}, TileId{8}};
    grass.autoTileRules.push_back({neighbors, ""});
    tileSet.SetTile(1, grass);
    Tile stone;
    stone.name = "Stone";
    tileSet.SetTile(2, stone);

    TileSetInspectorWidget inspector(&tileSet);
    auto* tileList = inspector.findChild<QWidget*>("TileSetTileList");
    auto* ruleList = inspector.findChild<QWidget*>("TileSetRulesList");
    auto* id = inspector.findChild<QSpinBox*>("TileSetTileId");
    auto* name = inspector.findChild<QLineEdit*>("TileSetTileName");
    ASSERT_NE(tileList, nullptr);
    ASSERT_NE(ruleList, nullptr);
    ASSERT_NE(id, nullptr);
    ASSERT_NE(name, nullptr);
    const auto tileRows = tileList->findChildren<QPushButton*>("TileSetTileSelect");
    ASSERT_EQ(tileRows.size(), 2);
    auto* grassName = tileRows[0]->findChild<QLabel*>("TileSetTileNameLabel");
    auto* stoneName = tileRows[1]->findChild<QLabel*>("TileSetTileNameLabel");
    ASSERT_NE(grassName, nullptr);
    ASSERT_NE(stoneName, nullptr);
    EXPECT_EQ(grassName->text(), "Grass");
    EXPECT_EQ(stoneName->text(), "Stone");
    EXPECT_NE(tileRows[0]->findChild<QLabel*>("TileSetTileThumbnail"), nullptr);
    EXPECT_EQ(id->value(), 1);
    EXPECT_EQ(name->text(), "Grass");
    EXPECT_EQ(ruleList->findChildren<QFrame*>("TileSetRuleCard").size(), 1);
    auto* north = ruleList->findChild<QToolButton*>("TileRuleNeighborN");
    ASSERT_NE(north, nullptr);
    EXPECT_EQ(north->text(), "1");
    auto* defaultSprite = inspector.findChild<QWidget*>("TileSetDefaultSprite");
    auto* rulePreview = inspector.findChild<QLabel*>("TileSetRulePreview");
    ASSERT_NE(defaultSprite, nullptr);
    ASSERT_NE(rulePreview, nullptr);
    EXPECT_NE(defaultSprite->findChild<QToolButton*>("ObjectRefPickerToggle"), nullptr);
    EXPECT_TRUE(rulePreview->acceptDrops());
    EXPECT_EQ(ruleList->findChild<QWidget*>("TileSetRuleSprite"), nullptr);

    tileRows[1]->click();
    EXPECT_EQ(id->value(), 2);
    EXPECT_EQ(name->text(), "Stone");
    QTRY_COMPARE(ruleList->findChildren<QFrame*>("TileSetRuleCard").size(), 0);
}

TEST(TileSetInspectorTest, ListControlsAddTileAndManageRules) {
    TileSet tileSet;
    TileSetInspectorWidget inspector(&tileSet);
    auto* tileList = inspector.findChild<QWidget*>("TileSetTileList");
    auto* ruleList = inspector.findChild<QWidget*>("TileSetRulesList");
    auto* name = inspector.findChild<QLineEdit*>("TileSetTileName");
    ASSERT_NE(tileList, nullptr);
    ASSERT_NE(ruleList, nullptr);
    ASSERT_NE(name, nullptr);

    auto* addTile = tileList->findChild<QPushButton*>("ListAddButton");
    ASSERT_NE(addTile, nullptr);
    addTile->click();
    ASSERT_NE(tileSet.GetTile(1), nullptr);
    inspector.Refresh();
    name->setText("Road");
    ASSERT_TRUE(QMetaObject::invokeMethod(name, "editingFinished"));
    EXPECT_EQ(tileSet.GetTile(1)->name, "Road");

    auto* addRule = ruleList->findChild<QPushButton*>("ListAddButton");
    ASSERT_NE(addRule, nullptr);
    addRule->click();
    ASSERT_EQ(tileSet.GetTile(1)->autoTileRules.size(), 1u);
    inspector.Refresh();
    QTRY_COMPARE(ruleList->findChildren<QToolButton*>("TileRuleNeighborN").size(), 1);
    auto* north = ruleList->findChild<QToolButton*>("TileRuleNeighborN");
    ASSERT_NE(north, nullptr);
    ASSERT_NE(north->menu(), nullptr);
    QAction* tileOption = nullptr;
    for (QAction* action : north->menu()->actions())
        if (action->text().contains("(ID 1)")) tileOption = action;
    ASSERT_NE(tileOption, nullptr);
    tileOption->trigger();
    NeighborPattern expected{};
    expected.fill(TileId{0});
    expected[0] = 1;
    EXPECT_TRUE(HasPattern(*tileSet.GetTile(1), expected));

    inspector.Refresh();
    north = ruleList->findChild<QToolButton*>("TileRuleNeighborN");
    ASSERT_NE(north, nullptr);
    QAction* anyOption = nullptr;
    for (QAction* action : north->menu()->actions())
        if (action->text().startsWith("Any")) anyOption = action;
    ASSERT_NE(anyOption, nullptr);
    anyOption->trigger();
    expected[0] = std::nullopt;
    EXPECT_TRUE(HasPattern(*tileSet.GetTile(1), expected));

    inspector.Refresh();
    auto* removeRule = ruleList->findChild<QPushButton*>("ListRemoveButton");
    ASSERT_NE(removeRule, nullptr);
    removeRule->click();
    QTRY_VERIFY(tileSet.GetTile(1)->autoTileRules.empty());

    auto* removeTile = tileList->findChild<QPushButton*>("ListRemoveButton");
    ASSERT_NE(removeTile, nullptr);
    removeTile->click();
    QTRY_VERIFY(tileSet.GetTile(1) == nullptr);
}

TEST(TileSetInspectorTest, EditingNeighborsKeepsRulePosition) {
    TileSet tileSet;
    Tile tile;
    NeighborPattern first{};
    first.fill(TileId{0});
    first[0] = TileId{1};
    NeighborPattern second{};
    second.fill(TileId{0});
    second[2] = TileId{1};
    tile.autoTileRules = {{first, "first"}, {second, "second"}};
    tileSet.SetTile(1, tile);

    TileSetInspectorWidget inspector(&tileSet);
    auto* ruleList = inspector.findChild<QWidget*>("TileSetRulesList");
    ASSERT_NE(ruleList, nullptr);
    const auto northCells = ruleList->findChildren<QToolButton*>("TileRuleNeighborN");
    ASSERT_EQ(northCells.size(), 2);
    QAction* anyOption = nullptr;
    for (QAction* action : northCells[0]->menu()->actions())
        if (action->text().startsWith("Any")) anyOption = action;
    ASSERT_NE(anyOption, nullptr);
    anyOption->trigger();

    const auto& rules = tileSet.GetTile(1)->autoTileRules;
    ASSERT_EQ(rules.size(), 2u);
    EXPECT_EQ(rules[0].spriteId, "first");
    EXPECT_EQ(rules[0].neighbors[0], std::nullopt);
    EXPECT_EQ(rules[1].spriteId, "second");
    EXPECT_EQ(rules[1].neighbors, second);
}

TEST(TileSetInspectorTest, BuildsLargeRuleListsAcrossEventLoopTurns) {
    TileSet tileSet;
    Tile tile;
    for (int i = 0; i < 20; ++i) {
        NeighborPattern pattern{};
        pattern.fill(TileId{0});
        pattern[0] = static_cast<TileId>(i + 1);
        tile.autoTileRules.push_back({pattern, ""});
    }
    tileSet.SetTile(1, tile);

    TileSetInspectorWidget inspector(&tileSet);
    auto* ruleList = inspector.findChild<QWidget*>("TileSetRulesList");
    ASSERT_NE(ruleList, nullptr);
    EXPECT_LT(ruleList->findChildren<QFrame*>("TileSetRuleCard").size(), 20);
    QTRY_COMPARE_WITH_TIMEOUT(ruleList->findChildren<QFrame*>("TileSetRuleCard").size(), 20, 1000);
}

TEST(TileSetInspectorTest, ChangingTilesStopsPendingRuleRows) {
    TileSet tileSet;
    Tile first;
    for (int i = 0; i < 20; ++i) {
        NeighborPattern pattern{};
        pattern.fill(TileId{0});
        pattern[0] = static_cast<TileId>(i + 1);
        first.autoTileRules.push_back({pattern, ""});
    }
    tileSet.SetTile(1, first);
    tileSet.SetTile(2, Tile{});

    TileSetInspectorWidget inspector(&tileSet);
    auto* tileList = inspector.findChild<QWidget*>("TileSetTileList");
    auto* ruleList = inspector.findChild<QWidget*>("TileSetRulesList");
    ASSERT_NE(tileList, nullptr);
    ASSERT_NE(ruleList, nullptr);
    const auto tiles = tileList->findChildren<QPushButton*>("TileSetTileSelect");
    ASSERT_EQ(tiles.size(), 2);
    tiles[1]->click();
    QTRY_COMPARE_WITH_TIMEOUT(ruleList->findChildren<QFrame*>("TileSetRuleCard").size(), 0, 1000);
}
