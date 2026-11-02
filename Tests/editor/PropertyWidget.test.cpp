#include <memory>

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QSignalSpy>
#include <QStyle>
#include <QStyleOptionButton>
#include <QTest>
#include <gtest/gtest.h>

#include "utils/PropertyWidget.hpp"

TEST(PropertyWidgetTest, ProgrammaticFloatValueDoesNotCallEditCallback) {
    Properties::PropDesc description;
    description.Range(0.0f, 10.0f);
    FloatPropertyWidget property(description);
    std::unique_ptr<QWidget> control(property.GetWidget());
    auto* spin = qobject_cast<QDoubleSpinBox*>(control.get());
    ASSERT_NE(spin, nullptr);
    QSignalSpy signalSpy(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged));
    ASSERT_TRUE(signalSpy.isValid());
    int edits = 0;
    property.onChanged = [&edits](float) { ++edits; };

    property.SetValue(3.0f);
    EXPECT_FLOAT_EQ(property.GetValue(), 3.0f);
    EXPECT_EQ(edits, 0);
    EXPECT_EQ(signalSpy.count(), 0);

    spin->setValue(4.0);
    EXPECT_FLOAT_EQ(property.GetValue(), 4.0f);
    EXPECT_EQ(edits, 1);
    EXPECT_EQ(signalSpy.count(), 1);
}

TEST(PropertyWidgetTest, ReadOnlyFloatRejectsKeyboardEdits) {
    Properties::PropDesc description;
    description.Range(0.0f, 10.0f).ReadOnly();
    FloatPropertyWidget property(description);
    std::unique_ptr<QWidget> control(property.GetWidget());
    auto* spin = qobject_cast<QDoubleSpinBox*>(control.get());
    ASSERT_NE(spin, nullptr);
    int edits = 0;
    property.onChanged = [&edits](float) { ++edits; };
    property.SetValue(5.0f);
    spin->show();
    spin->setFocus();

    QTest::keyClick(spin, Qt::Key_Up);
    EXPECT_TRUE(spin->isReadOnly());
    EXPECT_FLOAT_EQ(property.GetValue(), 5.0f);
    EXPECT_EQ(edits, 0);
}

TEST(PropertyWidgetTest, CheckboxSeparatesProgrammaticAndUserChanges) {
    Properties::PropDesc description;
    BoolPropertyWidget property(description);
    std::unique_ptr<QWidget> control(property.GetWidget());
    auto* checkbox = qobject_cast<QCheckBox*>(control.get());
    ASSERT_NE(checkbox, nullptr);
    QSignalSpy signalSpy(checkbox, &QCheckBox::toggled);
    ASSERT_TRUE(signalSpy.isValid());
    int edits = 0;
    property.onChanged = [&edits](bool) { ++edits; };

    property.SetValue(true);
    EXPECT_TRUE(property.GetValue());
    EXPECT_EQ(edits, 0);
    EXPECT_EQ(signalSpy.count(), 0);

    checkbox->resize(100, 30);
    checkbox->show();
    QStyleOptionButton option;
    option.initFrom(checkbox);
    const QRect indicator = checkbox->style()->subElementRect(
        QStyle::SE_CheckBoxIndicator, &option, checkbox);
    ASSERT_FALSE(indicator.isEmpty());
    QTest::mouseClick(checkbox, Qt::LeftButton, Qt::NoModifier, indicator.center());
    EXPECT_FALSE(property.GetValue());
    EXPECT_EQ(edits, 1);
    EXPECT_EQ(signalSpy.count(), 1);
}

TEST(PropertyWidgetTest, ReadOnlyCheckboxCannotBeToggledByClick) {
    Properties::PropDesc description;
    description.ReadOnly();
    BoolPropertyWidget property(description);
    std::unique_ptr<QWidget> control(property.GetWidget());
    auto* checkbox = qobject_cast<QCheckBox*>(control.get());
    ASSERT_NE(checkbox, nullptr);
    property.SetValue(true);
    checkbox->resize(100, 30);
    checkbox->show();

    QStyleOptionButton option;
    option.initFrom(checkbox);
    const QRect indicator = checkbox->style()->subElementRect(
        QStyle::SE_CheckBoxIndicator, &option, checkbox);
    ASSERT_FALSE(indicator.isEmpty());
    QTest::mouseClick(checkbox, Qt::LeftButton, Qt::NoModifier, indicator.center());
    EXPECT_FALSE(checkbox->isEnabled());
    EXPECT_TRUE(property.GetValue());
}
