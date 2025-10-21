#include "ConsoleGui.hpp"
#include <QPushButton>
ConsoleGui::ConsoleGui(QWidget* parent)
    : QWidget(parent)
{
    button = new QPushButton("ADD", this);
    button->resize(100, 40);
    resize(720, 300);
    Console::Get().Subscribe([this](){resize(200, 80);});
    connect(button, &QPushButton::clicked,  [](bool){
        Console::Comment("Resize");
    });

}

void ConsoleGui::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    int margin = 10;
    int x = width() - button->width() - margin;
    int y = height() - button->height() - margin;
    button->move(x, y);
}
