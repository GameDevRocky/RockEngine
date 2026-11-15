#include <algorithm>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QTest>
#include <QToolButton>
#include <gtest/gtest.h>

#include "engine/tilemap/TileSet.hpp"
#include "utils/TileSetInspectorWidget.hpp"
#include "utils/TileCollisionShapeEditor.hpp"

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
    EXPECT_EQ(ruleList->findChildren<QWidget*>("TileSetRuleCard").size(), 1);
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
    QTRY_COMPARE(ruleList->findChildren<QWidget*>("TileSetRuleCard").size(), 0);
}

TEST(TileSetInspectorTest, AddingTileKeepsExistingTileRow) {
    TileSet tileSet;
    Tile tile;
    tile.name = "Existing";
    tileSet.SetTile(1, tile);
    TileSetInspectorWidget inspector(&tileSet);
    auto* tileList = inspector.findChild<QWidget*>("TileSetTileList");
    ASSERT_NE(tileList, nullptr);
    auto rows = tileList->findChildren<QPushButton*>("TileSetTileSelect");
    ASSERT_EQ(rows.size(), 1);
    QPointer<QPushButton> existing = rows.front();
    auto* add = tileList->findChild<QPushButton*>("ListAddButton");
    ASSERT_NE(add, nullptr);

    add->click();
    QTRY_COMPARE(tileList->findChildren<QPushButton*>("TileSetTileSelect").size(), 2);
    rows = tileList->findChildren<QPushButton*>("TileSetTileSelect");
    EXPECT_EQ(rows.front(), existing);
    EXPECT_NE(tileSet.GetTile(2), nullptr);
}

TEST(TileSetInspectorTest, RemovingTileKeepsOtherTileRows) {
    TileSet tileSet;
    for (TileId id = 1; id <= 3; ++id) {
        Tile tile;
        tile.name = "Tile " + std::to_string(id);
        tileSet.SetTile(id, tile);
    }
    TileSetInspectorWidget inspector(&tileSet);
    auto* tileList = inspector.findChild<QWidget*>("TileSetTileList");
    ASSERT_NE(tileList, nullptr);
    auto rows = tileList->findChildren<QPushButton*>("TileSetTileSelect");
    ASSERT_EQ(rows.size(), 3);
    QPointer<QPushButton> first = rows[0];
    QPointer<QPushButton> third = rows[2];
    auto removeButtons = tileList->findChildren<QPushButton*>("ListRemoveButton");
    ASSERT_EQ(removeButtons.size(), 3);

    removeButtons[1]->click();
    QTRY_COMPARE(tileList->findChildren<QPushButton*>("TileSetTileSelect").size(), 2);
    rows = tileList->findChildren<QPushButton*>("TileSetTileSelect");
    EXPECT_EQ(rows[0], first);
    EXPECT_EQ(rows[1], third);
    EXPECT_EQ(tileSet.GetTile(2), nullptr);
}

TEST(TileSetInspectorTest, FooterButtonsAppendAndRemoveLastTile) {
    TileSet tileSet;
    Tile first;
    first.name = "First";
    tileSet.SetTile(1, first);
    TileSetInspectorWidget inspector(&tileSet);
    auto* tileList = inspector.findChild<QWidget*>("TileSetTileList");
    ASSERT_NE(tileList, nullptr);
    auto* append = tileList->findChild<QPushButton*>("ListAppendButton");
    auto* removeLast = tileList->findChild<QPushButton*>("ListRemoveLastButton");
    ASSERT_NE(append, nullptr);
    ASSERT_NE(removeLast, nullptr);

    append->click();
    QTRY_VERIFY(tileSet.GetTile(2) != nullptr);
    removeLast->click();
    QTRY_VERIFY(tileSet.GetTile(2) == nullptr);
    EXPECT_NE(tileSet.GetTile(1), nullptr);
}

TEST(TileSetInspectorTest, AddingRuleKeepsExistingRuleCard) {
    TileSet tileSet;
    Tile tile;
    tile.autoTileRules.push_back(AutoTileRule{});
    tileSet.SetTile(1, tile);
    TileSetInspectorWidget inspector(&tileSet);
    auto* ruleList = inspector.findChild<QWidget*>("TileSetRulesList");
    ASSERT_NE(ruleList, nullptr);
    auto cards = ruleList->findChildren<QWidget*>("TileSetRuleCard");
    ASSERT_EQ(cards.size(), 1);
    QPointer<QWidget> existing = cards.front();
    auto* add = ruleList->findChild<QPushButton*>("ListAddButton");
    ASSERT_NE(add, nullptr);

    add->click();
    QTRY_COMPARE(ruleList->findChildren<QWidget*>("TileSetRuleCard").size(), 2);
    cards = ruleList->findChildren<QWidget*>("TileSetRuleCard");
    EXPECT_EQ(cards.front(), existing);
    ASSERT_NE(tileSet.GetTile(1), nullptr);
    EXPECT_EQ(tileSet.GetTile(1)->autoTileRules.size(), 2);
}

TEST(TileSetInspectorTest, RemovingRuleKeepsOtherRuleCards) {
    TileSet tileSet;
    Tile tile;
    for (TileId id = 1; id <= 3; ++id) {
        AutoTileRule rule;
        rule.neighbors.fill(EMPTY_TILE_ID);
        rule.neighbors[0] = id;
        tile.autoTileRules.push_back(rule);
    }
    tileSet.SetTile(1, tile);
    TileSetInspectorWidget inspector(&tileSet);
    auto* ruleList = inspector.findChild<QWidget*>("TileSetRulesList");
    ASSERT_NE(ruleList, nullptr);
    auto cards = ruleList->findChildren<QWidget*>("TileSetRuleCard");
    ASSERT_EQ(cards.size(), 3);
    QPointer<QWidget> first = cards[0];
    QPointer<QWidget> third = cards[2];
    auto removeButtons = ruleList->findChildren<QPushButton*>("ListRemoveButton");
    ASSERT_EQ(removeButtons.size(), 3);

    removeButtons[1]->click();
    QTRY_COMPARE(ruleList->findChildren<QWidget*>("TileSetRuleCard").size(), 2);
    cards = ruleList->findChildren<QWidget*>("TileSetRuleCard");
    EXPECT_EQ(cards[0], first);
    EXPECT_EQ(cards[1], third);
    ASSERT_NE(tileSet.GetTile(1), nullptr);
    EXPECT_EQ(tileSet.GetTile(1)->autoTileRules.size(), 2);
}

TEST(TileSetInspectorTest, CollisionTypeEditsOnlyTheSelectedTile) {
    TileSet tileSet;
    Tile first;
    first.name = "First";
    Tile second;
    second.name = "Second";
    tileSet.SetTile(1, first);
    tileSet.SetTile(2, second);

    TileSetInspectorWidget inspector(&tileSet);
    auto* collision = inspector.findChild<QComboBox*>("TileSetCollisionType");
    auto* tileList = inspector.findChild<QWidget*>("TileSetTileList");
    ASSERT_NE(collision, nullptr);
    ASSERT_NE(tileList, nullptr);
    EXPECT_EQ(collision->currentData().toInt(), static_cast<int>(TileCollisionType::None));

    collision->setCurrentIndex(collision->findData(static_cast<int>(TileCollisionType::Grid)));
    ASSERT_NE(tileSet.GetTile(1), nullptr);
    ASSERT_NE(tileSet.GetTile(2), nullptr);
    EXPECT_EQ(tileSet.GetTile(1)->collisionType, TileCollisionType::Grid);
    EXPECT_EQ(tileSet.GetTile(2)->collisionType, TileCollisionType::None);

    const auto rows = tileList->findChildren<QPushButton*>("TileSetTileSelect");
    ASSERT_EQ(rows.size(), 2);
    rows[1]->click();
    EXPECT_EQ(collision->currentData().toInt(), static_cast<int>(TileCollisionType::None));
}

TEST(TileSetInspectorTest, RuleCardsExposeColliderAndOutputSelections) {
    TileSet tileSet;
    Tile tile;
    tile.collisionType = TileCollisionType::Grid;
    AutoTileRule rule;
    rule.spriteId = "fixed-sprite";
    rule.collisionType = TileCollisionType::Grid;
    tile.autoTileRules.push_back(rule);
    tileSet.SetTile(1, tile);

    TileSetInspectorWidget inspector(&tileSet);
    auto* output = inspector.findChild<QComboBox*>("TileRuleOutput");
    auto* collision = inspector.findChild<QComboBox*>("TileRuleCollision");
    auto* spriteList = inspector.findChild<QWidget*>("TileRuleSpriteList");
    ASSERT_NE(output, nullptr);
    ASSERT_NE(collision, nullptr);
    ASSERT_NE(spriteList, nullptr);
    EXPECT_EQ(output->currentData().toInt(), static_cast<int>(TileOutputType::Fixed));
    EXPECT_EQ(collision->currentData().toInt(), static_cast<int>(TileCollisionType::Grid));
    EXPECT_TRUE(spriteList->isHidden());

    output->setCurrentIndex(output->findData(static_cast<int>(TileOutputType::Random)));
    ASSERT_NE(tileSet.GetTile(1), nullptr);
    ASSERT_EQ(tileSet.GetTile(1)->autoTileRules.size(), 1u);
    EXPECT_EQ(tileSet.GetTile(1)->autoTileRules[0].outputType, TileOutputType::Random);
    EXPECT_EQ(tileSet.GetTile(1)->autoTileRules[0].spriteIds,
              std::vector<std::string>{"fixed-sprite"});
    EXPECT_FALSE(spriteList->isHidden());

    collision->setCurrentIndex(collision->findData(static_cast<int>(TileCollisionType::Sprite)));
    EXPECT_EQ(tileSet.GetTile(1)->autoTileRules[0].collisionType, TileCollisionType::Sprite);
}

TEST(TileSetInspectorTest, RuleCardPlacesSettingsBesidePreviewAndNeighbors) {
    TileSet tileSet;
    Tile tile;
    tile.autoTileRules.push_back(AutoTileRule{});
    tileSet.SetTile(1, tile);
    TileSetInspectorWidget inspector(&tileSet);
    auto* card = inspector.findChild<QWidget*>("TileSetRuleCard");
    ASSERT_NE(card, nullptr);
    EXPECT_STREQ(card->metaObject()->className(), "QWidget");
    auto* properties = card->findChild<QWidget*>("TileRuleProperties");
    auto* visuals = card->findChild<QWidget*>("TileRuleVisuals");
    auto* neighbors = card->findChild<QWidget*>("TileRuleNeighbors");
    auto* preview = card->findChild<QLabel*>("TileSetRulePreview");
    auto* output = card->findChild<QComboBox*>("TileRuleOutput");
    auto* collision = card->findChild<QComboBox*>("TileRuleCollision");
    auto* spriteList = card->findChild<QWidget*>("TileRuleSpriteList");
    auto* speed = card->findChild<QDoubleSpinBox*>("TileRuleMinSpeed");
    EXPECT_EQ(card->findChild<QToolButton*>("TileSetRuleToggle"), nullptr);
    ASSERT_NE(properties, nullptr);
    ASSERT_NE(visuals, nullptr);
    ASSERT_NE(neighbors, nullptr);
    ASSERT_NE(preview, nullptr);
    ASSERT_NE(output, nullptr);
    ASSERT_NE(collision, nullptr);
    ASSERT_NE(spriteList, nullptr);
    ASSERT_NE(speed, nullptr);

    ASSERT_NE(card->layout(), nullptr);
    auto* row = card->findChild<QWidget*>("TileRuleRow");
    ASSERT_NE(row, nullptr);
    EXPECT_EQ(card->layout()->itemAt(0)->widget(), row);
    EXPECT_EQ(row->layout()->itemAt(0)->widget(), properties);
    EXPECT_EQ(row->layout()->itemAt(1)->widget(), visuals);
    ASSERT_NE(visuals->layout(), nullptr);
    EXPECT_EQ(visuals->layout()->itemAt(0)->widget(), preview);
    EXPECT_EQ(visuals->layout()->itemAt(1)->widget(), neighbors);
    EXPECT_EQ(output->sizePolicy().horizontalPolicy(), QSizePolicy::Expanding);
    EXPECT_EQ(collision->sizePolicy().horizontalPolicy(), QSizePolicy::Expanding);
    EXPECT_EQ(output->minimumSizeHint().width(), 0);
    EXPECT_EQ(collision->minimumSizeHint().width(), 0);
    EXPECT_EQ(properties->layout()->itemAt(2)->widget(), spriteList);
    EXPECT_EQ(properties->layout()->itemAt(3)->widget(), speed->parentWidget());
    int boldLabels = 0;
    for (auto* label : properties->findChildren<QLabel*>())
        if (label->text() == "Output" || label->text() == "Collider") {
            EXPECT_TRUE(label->font().bold());
            EXPECT_EQ(label->minimumWidth(), 0);
            ++boldLabels;
        }
    EXPECT_EQ(boldLabels, 2);

    output->setCurrentIndex(output->findData(static_cast<int>(TileOutputType::Random)));
    EXPECT_FALSE(spriteList->isHidden());
    EXPECT_TRUE(speed->parentWidget()->isHidden());
    output->setCurrentIndex(output->findData(static_cast<int>(TileOutputType::Animation)));
    EXPECT_FALSE(speed->parentWidget()->isHidden());

}

TEST(TileSetInspectorTest, CustomPolygonEditorsPersistDefaultAndRulePoints) {
    TileSet tileSet;
    Tile tile;
    tile.autoTileRules.push_back(AutoTileRule{});
    ASSERT_TRUE(tileSet.SetTile(1, tile));
    TileSetInspectorWidget inspector(&tileSet);
    auto* defaultMode = inspector.findChild<QComboBox*>("TileSetCollisionType");
    auto* ruleMode = inspector.findChild<QComboBox*>("TileRuleCollision");
    auto* defaultEditor = inspector.findChild<TileCollisionShapeEditor*>(
        "TileSetDefaultCustomCollisionEditor");
    auto* ruleEditor = inspector.findChild<TileCollisionShapeEditor*>(
        "TileRuleCustomCollisionEditor");
    ASSERT_NE(defaultMode, nullptr);
    ASSERT_NE(ruleMode, nullptr);
    ASSERT_NE(defaultEditor, nullptr);
    ASSERT_NE(ruleEditor, nullptr);
    EXPECT_TRUE(defaultEditor->isHidden());
    EXPECT_TRUE(ruleEditor->isHidden());

    defaultMode->setCurrentIndex(defaultMode->findData(static_cast<int>(TileCollisionType::Custom)));
    ruleMode->setCurrentIndex(ruleMode->findData(static_cast<int>(TileCollisionType::Custom)));
    EXPECT_FALSE(defaultEditor->isHidden());
    EXPECT_FALSE(ruleEditor->isHidden());
    ASSERT_EQ(tileSet.GetTile(1)->collisionType, TileCollisionType::Custom);
    ASSERT_EQ(tileSet.GetTile(1)->autoTileRules[0].collisionType, TileCollisionType::Custom);

    const QPoint a = defaultEditor->VertexPosition(0).toPoint();
    const QPoint b = defaultEditor->VertexPosition(1).toPoint();
    QTest::mouseDClick(defaultEditor, Qt::LeftButton, Qt::NoModifier, (a + b) / 2);
    ASSERT_EQ(tileSet.GetTile(1)->customPolygon.size(), 5u);
    EXPECT_EQ(tileSet.GetTile(1)->autoTileRules[0].customPolygon.size(), 4u);

    const QPoint c = ruleEditor->VertexPosition(1).toPoint();
    const QPoint d = ruleEditor->VertexPosition(2).toPoint();
    QTest::mouseDClick(ruleEditor, Qt::LeftButton, Qt::NoModifier, (c + d) / 2);
    ASSERT_EQ(tileSet.GetTile(1)->autoTileRules[0].customPolygon.size(), 5u);
    QTest::mouseClick(ruleEditor, Qt::RightButton, Qt::NoModifier,
                      ruleEditor->VertexPosition(1).toPoint());
    EXPECT_EQ(tileSet.GetTile(1)->autoTileRules[0].customPolygon.size(), 4u);
}

TEST(TileSetInspectorTest, PolygonEditorRejectsConcaveDragAndEnforcesPointLimits) {
    TileCollisionShapeEditor editor;
    int changes = 0;
    editor.onChanged = [&](const TileCollisionPolygon&) { ++changes; };
    const QPoint start = editor.VertexPosition(0).toPoint();
    QTest::mousePress(&editor, Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseMove(&editor, QPoint(190, 80));
    QTest::mouseRelease(&editor, Qt::LeftButton, Qt::NoModifier, QPoint(190, 80));
    EXPECT_EQ(editor.Polygon(), DefaultTileCollisionPolygon());
    EXPECT_EQ(changes, 0);

    const QPoint edgeMidpoints[] = {{120, 180}, {180, 120}, {120, 60}, {60, 120}};
    for (int count = 4; count < 8; ++count) {
        QTest::mouseDClick(&editor, Qt::LeftButton, Qt::NoModifier,
                           edgeMidpoints[count - 4]);
        ASSERT_EQ(editor.Polygon().size(), static_cast<std::size_t>(count + 1));
    }
    const QPoint a = editor.VertexPosition(0).toPoint();
    const QPoint b = editor.VertexPosition(1).toPoint();
    QTest::mouseDClick(&editor, Qt::LeftButton, Qt::NoModifier, (a + b) / 2);
    EXPECT_EQ(editor.Polygon().size(), 8u);

    for (int attempt = 0; attempt < 5 && editor.Polygon().size() > 3; ++attempt) {
        const std::size_t before = editor.Polygon().size();
        const std::size_t vertex = before == 4 ? 3 : 1;
        QTest::mouseClick(&editor, Qt::RightButton, Qt::NoModifier,
                          editor.VertexPosition(vertex).toPoint());
        ASSERT_EQ(editor.Polygon().size(), before - 1);
    }
    EXPECT_EQ(editor.Polygon().size(), 3u);
    QTest::mouseClick(&editor, Qt::RightButton, Qt::NoModifier,
                      editor.VertexPosition(1).toPoint());
    EXPECT_EQ(editor.Polygon().size(), 3u);
}

TEST(TileSetInspectorTest, PolygonEditorClampsDraggedPointsToSpriteBounds) {
    TileCollisionShapeEditor editor;
    editor.SetPolygon({{-0.25f, -0.25f}, {0.25f, -0.25f},
                       {0.25f, 0.25f}, {-0.25f, 0.25f}});
    QTest::mousePress(&editor, Qt::LeftButton, Qt::NoModifier,
                      editor.VertexPosition(0).toPoint());
    QTest::mouseMove(&editor, QPoint(0, 230));
    QTest::mouseRelease(&editor, Qt::LeftButton, Qt::NoModifier, QPoint(0, 230));
    EXPECT_EQ(editor.Polygon()[0], glm::vec2(-0.5f, -0.5f));

    QTest::mousePress(&editor, Qt::LeftButton, Qt::NoModifier,
                      editor.VertexPosition(2).toPoint());
    QTest::mouseMove(&editor, QPoint(230, 0));
    QTest::mouseRelease(&editor, Qt::LeftButton, Qt::NoModifier, QPoint(230, 0));
    EXPECT_EQ(editor.Polygon()[2], glm::vec2(0.5f, 0.5f));
}

TEST(TileSetInspectorTest, CaptionsAndTileNamesAreBold) {
    TileSet tileSet;
    Tile tile;
    tile.name = "Grass";
    tile.autoTileRules.push_back(AutoTileRule{});
    tileSet.SetTile(1, tile);
    TileSetInspectorWidget inspector(&tileSet);
    const auto labels = inspector.findChildren<QLabel*>();
    for (const char* caption : {"Tiles", "Grass", "Tile ID", "Name", "Default Sprite",
                                "Default Collider", "Tiling Rules", "Collider", "Output",
                                "Speed", "Tile", "None"}) {
        const auto it = std::find_if(labels.begin(), labels.end(),
            [caption](const QLabel* label) { return label->text() == caption; });
        ASSERT_NE(it, labels.end()) << caption;
        EXPECT_TRUE((*it)->font().bold()) << caption;
    }
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
    EXPECT_LT(ruleList->findChildren<QWidget*>("TileSetRuleCard").size(), 20);
    QTRY_COMPARE_WITH_TIMEOUT(ruleList->findChildren<QWidget*>("TileSetRuleCard").size(), 20, 1000);
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
    QTRY_COMPARE_WITH_TIMEOUT(ruleList->findChildren<QWidget*>("TileSetRuleCard").size(), 0, 1000);
}
