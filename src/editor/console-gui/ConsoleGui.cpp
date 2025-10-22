#include "ConsoleGui.hpp"
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QScrollArea>
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

ConsoleGui::ConsoleGui(QWidget* parent)
    : QWidget(parent)
{
    // Scroll container setup
    content = new QScrollArea(this);
    QWidget* scrollWidget = new QWidget();
    QVBoxLayout* scrollLayout = new QVBoxLayout(scrollWidget);
    scrollLayout->setContentsMargins(5, 5, 5, 5);
    scrollWidget->setLayout(scrollLayout);

    content->setWidget(scrollWidget);
    content->setWidgetResizable(true);

    button = new QPushButton("ADD", this);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(content);
    mainLayout->addWidget(button);
    setLayout(mainLayout);

    resize(720, 300);

    Console::Get().Subscribe([this]() { GenerateWidgets(); });

    connect(button, &QPushButton::clicked, [](bool) {
        float elapsed = TimeManager::Get().ElapsedTime();
        std::string msg = "Elapsed time: " + std::to_string(elapsed);
        Console::Alert("Testing Out the Length of the widget and How much text it can take and handle. I am just adding text to see how well it looks with a lot of text. I have an interview tmr and I'mm kinda nervous. Wiish me Luck!");
    });
}

void ConsoleGui::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
}


void ConsoleGui::GenerateWidgets()
{
    QWidget* scrollWidget = content->widget();
    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(scrollWidget->layout());

    QLayoutItem* item;
    while ((item = layout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

std::vector<std::pair<std::string, Message>> allMessages;

auto addMessages = [&allMessages](const auto& container) {
    allMessages.insert(allMessages.end(), container.begin(), container.end());
};

addMessages(Console::GetComments());
addMessages(Console::GetWarnings());
addMessages(Console::GetAlerts());

std::sort(allMessages.begin(), allMessages.end(),
          [](const auto& a, const auto& b) {
              return a.second.time_stamp < b.second.time_stamp;
          });

for (auto& [text, message] : allMessages) {
    QWidget* widget = new QWidget(scrollWidget);

    widget->setStyleSheet(
        message.type == "alert" ? ALERT_STYLESHEET :
        message.type == "warning" ? WARNING_STYLESHEET :
        COMMENT_STYLESHEET
    );
    widget->setObjectName("body");
    widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);


    QLabel* file_name = new QLabel(QString::fromStdString(message.file_name), widget);
    file_name->setObjectName("file_name");

    std::string typeUpper = message.type;
    std::transform(typeUpper.begin(), typeUpper.end(), typeUpper.begin(),
                   [](unsigned char c){ return std::toupper(c); });
    std::string labelText = "[" + typeUpper + "]: " + message.text;

    QLabel* label = new QLabel(QString::fromStdString(labelText), widget);
    label->setWordWrap(true);               
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QLabel* count = new QLabel(QString::number(message.count), widget);
    count->setObjectName("countLabel");
    QVBoxLayout* vbox = new QVBoxLayout(widget);
    vbox->addWidget(label);
    vbox->setSizeConstraint(QLayout::SetMinAndMaxSize); 

    QHBoxLayout* bottomLayout = new QHBoxLayout();
    bottomLayout->addWidget(file_name);
    bottomLayout->addStretch();
    bottomLayout->addWidget(count);
    vbox->addLayout(bottomLayout);
    vbox->setContentsMargins(10, 10, 10, 10);

    widget->setLayout(vbox);

    layout->addWidget(widget);
}

layout->addStretch();

}
