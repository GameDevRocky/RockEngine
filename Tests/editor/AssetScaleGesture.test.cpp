#include <QApplication>
#include <QListView>
#include <QListWidget>
#include <QSlider>
#include <QWheelEvent>
#include <gtest/gtest.h>

#include "dock-widgets/FolderViewGui.hpp"
#include "utils/AssetPickerWidget.hpp"

namespace {
void SendWheel(QWidget* target, int delta,
               Qt::KeyboardModifiers modifiers = Qt::ControlModifier)
{
    QWheelEvent event(QPointF(4, 4), QPointF(4, 4), QPoint(), QPoint(0, delta),
                      Qt::NoButton, modifiers, Qt::NoScrollPhase, false);
    QApplication::sendEvent(target, &event);
}
}

TEST(AssetScaleGestureTest, FolderViewCtrlWheelUpdatesExistingSizeSlider)
{
    auto* folder = FolderViewGui::Get();
    auto* slider = folder->findChild<QSlider*>("FolderCellSizeSlider");
    auto* list = folder->findChild<QListView*>();
    ASSERT_NE(slider, nullptr);
    ASSERT_NE(list, nullptr);

    slider->setValue(72);
    SendWheel(list->viewport(), 120);
    EXPECT_EQ(slider->value(), 80);
    SendWheel(list->viewport(), -120);
    EXPECT_EQ(slider->value(), 72);

    SendWheel(list->viewport(), 120, Qt::NoModifier);
    EXPECT_EQ(slider->value(), 72);
}

TEST(AssetScaleGestureTest, AssetReferencePickerCtrlWheelUpdatesExistingSizeSlider)
{
    AssetPickerWidget picker({{"First", "first"}, {"Second", "second"}});
    auto* list = picker.findChild<QListWidget*>("AssetPickerList");
    auto* slider = picker.findChild<QSlider*>("AssetPickerCellSizeSlider");
    ASSERT_NE(list, nullptr);
    ASSERT_NE(slider, nullptr);

    picker.setCellSize(72);
    SendWheel(list->viewport(), 120);
    EXPECT_EQ(picker.cellSize(), 80);
    SendWheel(list->viewport(), -120);
    EXPECT_EQ(picker.cellSize(), 72);

    SendWheel(list->viewport(), -120, Qt::NoModifier);
    EXPECT_EQ(picker.cellSize(), 72);
}
