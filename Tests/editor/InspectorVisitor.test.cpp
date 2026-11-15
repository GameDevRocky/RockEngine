#include <gtest/gtest.h>
#include <memory>
#include <QGraphicsOpacityEffect>
#include <QComboBox>
#include <QLineEdit>

#include "EditorSceneFixture.hpp"
#include "Engine.hpp"
#include "engine/components/TileMapRenderer.hpp"
#include "engine/core/Container.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/core/LayerManager.hpp"
#include "engine/rendering/core/Material.hpp"
#include "utils/InspectorVisitor.hpp"
#include "utils/InlineColorPicker.hpp"

TEST(InspectorVisitorTest, DefersRowsForObjectsAndAssets) {
    GameObject object;
    InspectorVisitor objectVisitor(true);
    std::unique_ptr<QWidget> objectContent(objectVisitor.GetContent());
    object.Accept(&objectVisitor);
    ASSERT_TRUE(objectVisitor.HasPendingRows());
    EXPECT_EQ(objectVisitor.GetContent()->layout()->count(), 0);
    objectVisitor.BuildNextRow();
    EXPECT_GT(objectVisitor.GetContent()->layout()->count(), 0);
    EXPECT_FALSE(objectVisitor.GetContent()->findChildren<QGraphicsOpacityEffect*>().empty());

    Material material;
    InspectorVisitor assetVisitor(true);
    std::unique_ptr<QWidget> assetContent(assetVisitor.GetContent());
    material.Accept(&assetVisitor);
    ASSERT_TRUE(assetVisitor.HasPendingRows());
    EXPECT_EQ(assetVisitor.GetContent()->layout()->count(), 0);
    assetVisitor.BuildNextRow();
    EXPECT_GT(assetVisitor.GetContent()->layout()->count(), 0);
    EXPECT_FALSE(assetVisitor.GetContent()->findChildren<QGraphicsOpacityEffect*>().empty());
}

TEST(InspectorVisitorTest, TileMapRendererUsesSortingLayerChoices) {
    auto* layers = Engine::Get()->GetActiveContainer()->FindSystem<LayerManager>();
    ASSERT_NE(layers, nullptr);
    ASSERT_GE(layers->GetLayers().size(), 2u);

    TileMapRenderer renderer;
    InspectorVisitor visitor;
    std::unique_ptr<QWidget> content(visitor.GetContent());
    visitor.Visit(&renderer);

    QComboBox* layerCombo = nullptr;
    for (QComboBox* combo : content->findChildren<QComboBox*>()) {
        if (combo->count() != static_cast<int>(layers->GetLayers().size())) continue;
        bool matches = true;
        for (const auto& layer : layers->GetLayers())
            matches &= combo->findText(QString::fromStdString(layer.name)) >= 0;
        if (matches) layerCombo = combo;
    }
    ASSERT_NE(layerCombo, nullptr);
    EXPECT_EQ(layerCombo->currentText().toStdString(), renderer.GetSortingLayer());

    const auto& target = layers->GetLayers().front().name == renderer.GetSortingLayer()
        ? layers->GetLayers().back() : layers->GetLayers().front();
    layerCombo->setCurrentIndex(layerCombo->findText(QString::fromStdString(target.name)));
    EXPECT_EQ(renderer.GetSortingLayer(), target.name);
}

class TileMapRendererInspectorTest : public EditorSceneTest {};

TEST_F(TileMapRendererInspectorTest, ColorPickerEditsRefreshesAndSupportsUndo) {
    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* renderer = new TileMapRenderer();
    object->AddComponent(renderer);
    const glm::vec4 initial(0.25f, 0.5f, 0.75f, 0.8f);
    renderer->SetColor(initial);
    auto* undo = editorContainer->FindSystem<UndoSystem>();
    ASSERT_NE(undo, nullptr);
    undo->Clear();
    struct ClearHistory { UndoSystem* undo; ~ClearHistory() { undo->Clear(); } } clear{undo};

    InspectorVisitor visitor;
    std::unique_ptr<QWidget> content(visitor.GetContent());
    visitor.Visit(renderer);
    auto* picker = dynamic_cast<InlineColorPicker*>(content->findChild<QWidget*>("InlineColorPicker"));
    ASSERT_NE(picker, nullptr);
    EXPECT_NEAR(picker->GetColor().redF(), initial.r, 0.0001f);
    EXPECT_NEAR(picker->GetColor().alphaF(), initial.a, 0.0001f);
    auto* hex = content->findChild<QLineEdit*>("ColorHexEdit");
    ASSERT_NE(hex, nullptr);
    hex->setText("#33669980");
    ASSERT_TRUE(QMetaObject::invokeMethod(hex, "editingFinished", Qt::DirectConnection));
    const glm::vec4 selected(51.0f / 255, 102.0f / 255, 153.0f / 255, 128.0f / 255);
    EXPECT_EQ(renderer->GetColor(), selected);

    undo->Undo();
    EXPECT_EQ(renderer->GetColor(), initial);
    for (const auto& refresh : visitor.GetRefreshers()) refresh();
    EXPECT_NEAR(picker->GetColor().redF(), initial.r, 0.0001f);
    EXPECT_NEAR(picker->GetColor().alphaF(), initial.a, 0.0001f);
    undo->Redo();
    EXPECT_EQ(renderer->GetColor(), selected);

    renderer->SetColor({0, 1, 0, 1});
    for (const auto& refresh : visitor.GetRefreshers()) refresh();
    EXPECT_EQ(picker->GetColor(), QColor(Qt::green));
    for (const auto& [source, subscription] : visitor.GetSubscriptions())
        source->Unsubscribe(subscription);
}
