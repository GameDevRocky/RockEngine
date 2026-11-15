#include <gtest/gtest.h>

#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QTest>
#include <QToolButton>

#include "utils/ProperyFactory.hpp"

TEST(ListPropertyWidgetTest, AddingAndRefreshingKeepsExistingEditors) {
    ListPropertyWidget<std::string> list(Properties::PropDesc().Tag(Properties::Tags::LIST));
    list.SetValue({"first", "second"});
    QWidget* container = list.GetWidget();
    auto editors = container->findChildren<QLineEdit*>();
    ASSERT_EQ(editors.size(), 2);
    QPointer<QLineEdit> first = editors[0];
    QPointer<QLineEdit> second = editors[1];

    auto* add = container->findChild<QPushButton*>("ListAddButton");
    auto* toggle = container->findChild<QToolButton*>("CollapseToggleButton");
    ASSERT_NE(add, nullptr);
    ASSERT_NE(toggle, nullptr);
    add->click();

    editors = container->findChildren<QLineEdit*>();
    ASSERT_EQ(editors.size(), 3);
    EXPECT_EQ(editors[0], first);
    EXPECT_EQ(editors[1], second);
    EXPECT_EQ(toggle->text(), "3 items");

    list.SetValue({"updated", "second", "third", "fourth"});
    editors = container->findChildren<QLineEdit*>();
    ASSERT_EQ(editors.size(), 4);
    EXPECT_EQ(editors[0], first);
    EXPECT_EQ(editors[1], second);
    EXPECT_EQ(editors[0]->text(), "updated");
    EXPECT_EQ(editors[3]->text(), "fourth");
}

TEST(ListPropertyWidgetTest, AddingWhileRowsArePendingKeepsEarlierEditors) {
    ListPropertyWidget<std::string> list(Properties::PropDesc().Tag(Properties::Tags::LIST));
    list.SetValue(std::vector<std::string>(20, "entry"));
    QWidget* container = list.GetWidget();
    auto editors = container->findChildren<QLineEdit*>();
    ASSERT_EQ(editors.size(), 4);
    QPointer<QLineEdit> first = editors.front();
    auto* add = container->findChild<QPushButton*>("ListAddButton");
    ASSERT_NE(add, nullptr);

    add->click();
    QTRY_COMPARE(container->findChildren<QLineEdit*>().size(), 21);
    EXPECT_EQ(container->findChildren<QLineEdit*>().front(), first);
    EXPECT_EQ(list.GetValue().size(), 21);
}

TEST(ListPropertyWidgetTest, RemovingMiddleRowKeepsRemainingEditorsAndBindings) {
    ListPropertyWidget<std::string> list(Properties::PropDesc().Tag(Properties::Tags::LIST));
    list.SetValue({"first", "second", "third"});
    QWidget* container = list.GetWidget();
    auto editors = container->findChildren<QLineEdit*>();
    ASSERT_EQ(editors.size(), 3);
    QPointer<QLineEdit> first = editors[0];
    QPointer<QLineEdit> third = editors[2];
    auto removeButtons = container->findChildren<QPushButton*>("ListRemoveButton");
    ASSERT_EQ(removeButtons.size(), 3);

    removeButtons[1]->click();
    QTRY_COMPARE(container->findChildren<QLineEdit*>().size(), 2);
    editors = container->findChildren<QLineEdit*>();
    EXPECT_EQ(editors[0], first);
    EXPECT_EQ(editors[1], third);
    EXPECT_EQ(list.GetValue(), (std::vector<std::string>{"first", "third"}));

    editors[1]->setText("edited");
    EXPECT_EQ(list.GetValue(), (std::vector<std::string>{"first", "edited"}));

    removeButtons = container->findChildren<QPushButton*>("ListRemoveButton");
    ASSERT_EQ(removeButtons.size(), 2);
    removeButtons[1]->click();
    QTRY_COMPARE(container->findChildren<QLineEdit*>().size(), 1);
    EXPECT_EQ(container->findChildren<QLineEdit*>().front(), first);
    EXPECT_EQ(list.GetValue(), (std::vector<std::string>{"first"}));
}

TEST(ListPropertyWidgetTest, FooterButtonsAppendAndRemoveOnlyTheLastRow) {
    ListPropertyWidget<std::string> list(Properties::PropDesc().Tag(Properties::Tags::LIST));
    list.SetValue({"first", "second"});
    QWidget* container = list.GetWidget();
    auto* append = container->findChild<QPushButton*>("ListAppendButton");
    auto* removeLast = container->findChild<QPushButton*>("ListRemoveLastButton");
    ASSERT_NE(append, nullptr);
    ASSERT_NE(removeLast, nullptr);
    EXPECT_EQ(append->size(), QSize(20, 20));
    EXPECT_EQ(removeLast->size(), QSize(20, 20));
    EXPECT_FALSE(append->icon().isNull());
    EXPECT_FALSE(removeLast->icon().isNull());
    EXPECT_EQ(append->iconSize(), QSize(14, 14));
    EXPECT_EQ(removeLast->iconSize(), QSize(14, 14));
    auto editors = container->findChildren<QLineEdit*>();
    ASSERT_EQ(editors.size(), 2);
    QPointer<QLineEdit> first = editors[0];
    QPointer<QLineEdit> second = editors[1];

    append->click();
    EXPECT_EQ(list.GetValue().size(), 3);
    removeLast->click();
    QTRY_COMPARE(container->findChildren<QLineEdit*>().size(), 2);
    editors = container->findChildren<QLineEdit*>();
    EXPECT_EQ(editors[0], first);
    EXPECT_EQ(editors[1], second);
    EXPECT_EQ(list.GetValue(), (std::vector<std::string>{"first", "second"}));

    removeLast->click();
    QTRY_COMPARE(container->findChildren<QLineEdit*>().size(), 1);
    removeLast->click();
    QTRY_COMPARE(container->findChildren<QLineEdit*>().size(), 0);
    EXPECT_FALSE(removeLast->isEnabled());
}

TEST(ListPropertyWidgetTest, FooterRemoveHandlesPendingRowsAndReadOnlyLists) {
    ListPropertyWidget<std::string> list(Properties::PropDesc().Tag(Properties::Tags::LIST));
    list.SetValue(std::vector<std::string>(20, "entry"));
    auto* removeLast = list.GetWidget()->findChild<QPushButton*>("ListRemoveLastButton");
    ASSERT_NE(removeLast, nullptr);
    removeLast->click();
    QTRY_COMPARE(list.GetWidget()->findChildren<QLineEdit*>().size(), 19);
    EXPECT_EQ(list.GetValue().size(), 19);

    ListPropertyWidget<std::string> readOnly(
        Properties::PropDesc().Tag(Properties::Tags::LIST).ReadOnly());
    EXPECT_EQ(readOnly.GetWidget()->findChild<QPushButton*>("ListAppendButton"), nullptr);
    EXPECT_EQ(readOnly.GetWidget()->findChild<QPushButton*>("ListRemoveLastButton"), nullptr);
}
