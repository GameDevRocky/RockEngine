#include "dock-widgets/ConsoleGui.hpp"
#include "engine/core/TimeManager.hpp"


ConsoleGui::ConsoleGui(QWidget* parent) : QWidget(parent)
{
    content = new QScrollArea(this);
    QWidget* scrollWidget = new QWidget();
    scrollLayout = new QVBoxLayout(scrollWidget);
    scrollLayout->setContentsMargins(4, 4, 4, 4);
    scrollLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);
    scrollLayout->setSpacing(4);
    scrollLayout->setAlignment(Qt::AlignTop);

    scrollWidget->setLayout(scrollLayout);

    content->setWidget(scrollWidget);
    content->setWidgetResizable(true);

    clear_button = new QPushButton("Clear", this);
    clear_button->setStyleSheet("font-size : 13px");
    QWidget* content_bar = new QWidget(this);
    QHBoxLayout* bar_layout = new QHBoxLayout(this);
    content_bar->setLayout(bar_layout);
    bar_layout->setContentsMargins(0,0,0,0);
    content_bar->setContentsMargins(0,0,0,0);
    bar_layout->addStretch();
    bar_layout->addWidget(clear_button);
    clear_button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    content->setAlignment(Qt::AlignTop);
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(content);
    mainLayout->addWidget(content_bar);
    mainLayout->setContentsMargins(0,0,0,0);
    setLayout(mainLayout);

    resize(720, 300);

    Console::Get().Subscribe([this]() { GenerateWidgets(); });

    connect(clear_button, &QPushButton::clicked, [](bool) {
        Console::Clear();
    });
    GenerateWidgets();
}
void ConsoleGui::Init(){
    
}

void ConsoleGui::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
}

void ConsoleGui::GenerateWidgets()
{
    auto& messages = Console::Get().GetMessages();
    for (auto& [key, msg] : messages) {
        if (message_widgets.find(key) == message_widgets.end()) {
            MessageGui* gui = new MessageGui(this, msg);
            scrollLayout->addWidget(gui);
            message_widgets.emplace(key, gui);
        }     
    }  
}