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
#include <QProcess>

#include "engine/debug/Console.hpp"
#include "utils/MessageGui.hpp"


class ConsoleGui : public QWidget {
    Q_OBJECT

public:
    static ConsoleGui* Get() {
        static ConsoleGui* instance = nullptr;
        if (!instance) {
            instance = new ConsoleGui(nullptr);
        }
        return instance;
    }
    std::unordered_map<std::string, MessageGui*> message_widgets;
    void Init();
protected:
    void resizeEvent(QResizeEvent* event) override;

    // Full rebuild from the message map. Only for first construction, which has to pick up
    // anything logged before this panel existed -- it is O(messages) and must not be run
    // per message. AddWidget is the incremental path.
    void GenerateWidgets();

    // Add the widget for exactly one message key, if it does not already have one.
    void AddWidget(const std::string& key);

private:
    explicit ConsoleGui(QWidget* parent = nullptr);

    QPushButton* clear_button;
    QScrollArea* content;
    QVBoxLayout* scrollLayout;


};
