#include "ConsoleGui.hpp"
#include "engine/core/TimeManager.hpp"

constexpr const char* COMMENT_STYLESHEET = R"(
    QWidget#body{
        border: 1px solid rgba(113, 113, 113, 1);
    }
    QWidget {
        background-color: rgba(37, 37, 37, 1);
        color: #ffffffff;
        border-radius: 5px;
    }
    QLabel {
        font-family: Consolas;
        font-size: 14px;
    }
    QLabel#countLabel {
        color: rgba(117, 180, 122, 1);
        font-weight: bold;
    }
    QLabel#file_name {
        color: rgba(255, 255, 255, 1);
        font-weight: bold;
    }
)";

constexpr const char* WARNING_STYLESHEET = R"(
    QWidget#body{
        border: 1px solid rgba(255, 140, 0, 1);
    }
    QWidget {
        background-color: rgba(100, 75, 45, 1);
        color: #ffffffff;
        border-radius: 5px;
    }
    QLabel {
        font-family: Consolas;
        font-size: 14px;
    }
    QLabel#countLabel {
        color: rgba(255, 196, 0, 0.77);
        font-weight: bold;
    }
    QLabel#file_name {
        color: rgba(255, 255, 255, 1);
        font-weight: bold;
    }
)";

constexpr const char* ALERT_STYLESHEET = R"(
    QWidget#body{
        border: 1px solid rgba(255, 0, 0, 1);
    }
    QWidget {
        background-color: rgba(69, 38, 38, 1);
        color: #ffffffff;
        border-radius: 5px;
    }
    QLabel {
        font-family: Consolas;
        font-size: 14px;
    }
    QLabel#countLabel {
        color: rgba(180, 117, 117, 1);
        font-weight: bold;
    }
    QLabel#file_name {
    
        color: rgba(255, 255, 255, 1);
        font-weight: bold;
    }
)";

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
    bar_layout->addWidget(clear_button);
    content_bar->setLayout(bar_layout);
    bar_layout->setContentsMargins(0,0,0,0);
    bar_layout->addStretch();
    clear_button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    content->setAlignment(Qt::AlignTop);
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(content);
    mainLayout->addWidget(content_bar);
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