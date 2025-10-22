#pragma once
#include <QWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QResizeEvent>
#include "engine/debug/Console.hpp"

class ConsoleGui : public QWidget {
    Q_OBJECT

public:
    static ConsoleGui* Get() {
        static ConsoleGui* instance = new ConsoleGui(nullptr);
        return instance;
    }

protected:
    void resizeEvent(QResizeEvent* event) override;
    void GenerateWidgets();

private:
    explicit ConsoleGui(QWidget* parent = nullptr);
    QPushButton* button;
    QScrollArea* content;


};
