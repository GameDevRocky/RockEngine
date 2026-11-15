#include <QApplication>
#include <QDir>
#include <QPlainTextEdit>
#include <QTemporaryFile>
#include <QTextBlock>
#include <QWheelEvent>
#include <gtest/gtest.h>

#include "EditorSceneFixture.hpp"
#include "dock-widgets/InspectorGui.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/core/SelectionManager.hpp"
#include "utils/SourcePreviewEdit.hpp"

class InspectorFilePreviewTest : public EditorSceneTest {};

TEST_F(InspectorFilePreviewTest, ShowsTextFileAndClearsLiveSelection)
{
    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* selection = editorContainer->FindSystem<SelectionManager>();
    ASSERT_NE(selection, nullptr);
    selection->Select(object->GetID());

    QTemporaryFile file(QDir::tempPath() + "/RockEnginePreviewXXXXXX.py");
    ASSERT_TRUE(file.open());
    const QByteArray source("class Player:\n    speed: float = 5.0\n");
    ASSERT_EQ(file.write(source), source.size());
    file.flush();
    file.close();

    auto* inspector = InspectorGui::Get();
    inspector->InspectFile(file.fileName());

    EXPECT_TRUE(selection->GetSelectedId().empty());
    auto* preview = dynamic_cast<SourcePreviewEdit*>(
        inspector->findChild<QPlainTextEdit*>("InspectorFilePreview"));
    ASSERT_NE(preview, nullptr);
    EXPECT_TRUE(preview->isReadOnly());
    EXPECT_EQ(preview->horizontalScrollBarPolicy(), Qt::ScrollBarAlwaysOff);
    EXPECT_EQ(preview->verticalScrollBarPolicy(), Qt::ScrollBarAlwaysOff);
    EXPECT_EQ(preview->toPlainText(), QString::fromUtf8(source));
    EXPECT_EQ(preview->LanguageName(), "Python");
    ASSERT_NE(preview->document()->firstBlock().layout(), nullptr);
    EXPECT_FALSE(preview->document()->firstBlock().layout()->formats().isEmpty());

    const int initialPointSize = preview->PreviewFontPointSize();
    QWheelEvent zoomIn(QPointF(4, 4), QPointF(4, 4), QPoint(), QPoint(0, 120),
                       Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(preview->viewport(), &zoomIn);
    EXPECT_EQ(preview->PreviewFontPointSize(), initialPointSize + 1);

    QWheelEvent zoomOut(QPointF(4, 4), QPointF(4, 4), QPoint(), QPoint(0, -120),
                        Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(preview->viewport(), &zoomOut);
    EXPECT_EQ(preview->PreviewFontPointSize(), initialPointSize);
}

TEST_F(InspectorFilePreviewTest, RefusesToRenderBinaryDataAsText)
{
    QTemporaryFile file;
    ASSERT_TRUE(file.open());
    const QByteArray binary("abc\0def", 7);
    ASSERT_EQ(file.write(binary), binary.size());
    file.flush();
    file.close();

    auto* inspector = InspectorGui::Get();
    inspector->InspectFile(file.fileName());

    auto* preview = dynamic_cast<SourcePreviewEdit*>(
        inspector->findChild<QPlainTextEdit*>("InspectorFilePreview"));
    ASSERT_NE(preview, nullptr);
    EXPECT_TRUE(preview->toPlainText().contains("binary data"));
}

TEST_F(InspectorFilePreviewTest, HighlightsYamlDescriptors)
{
    QTemporaryFile file(QDir::tempPath() + "/RockEnginePreviewXXXXXX.scene");
    ASSERT_TRUE(file.open());
    const QByteArray source("id: preview-scene\ngameobjects: []\n");
    ASSERT_EQ(file.write(source), source.size());
    file.close();

    auto* inspector = InspectorGui::Get();
    inspector->InspectFile(file.fileName());

    auto* preview = dynamic_cast<SourcePreviewEdit*>(
        inspector->findChild<QPlainTextEdit*>("InspectorFilePreview"));
    ASSERT_NE(preview, nullptr);
    EXPECT_EQ(preview->LanguageName(), "YAML");
    ASSERT_NE(preview->document()->firstBlock().layout(), nullptr);
    EXPECT_FALSE(preview->document()->firstBlock().layout()->formats().isEmpty());
}
