#pragma once
#include <QWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QLabel>
#include <QScrollBar>
#include <unordered_set>
#include <iostream>
#include "engine/debug/Console.hpp"
#include "MessageGui.hpp"


class ConsoleGui : public QWidget {
    Q_OBJECT

public:
    static ConsoleGui* Get() {
        static ConsoleGui* instance = new ConsoleGui(nullptr);
        return instance;
    }
    std::unordered_map<std::string, MessageGui*> message_widgets;

protected:
    void resizeEvent(QResizeEvent* event) override;
    void GenerateWidgets();


private:
    explicit ConsoleGui(QWidget* parent = nullptr);

    QPushButton* clear_button;
    QScrollArea* content;
    QVBoxLayout* scrollLayout;  // ✅ add this


};
