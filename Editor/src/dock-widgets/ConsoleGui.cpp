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
    QWidget* content_bar = new QWidget(this);
    QHBoxLayout* bar_layout = new QHBoxLayout(this);
    content_bar->setLayout(bar_layout);
    bar_layout->setContentsMargins(4,4,4,4);
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
    resize(720, 200);
    // Payload overload: Console sends the key of the message it just touched, so this adds
    // one widget instead of rescanning the whole map looking for what changed. This runs
    // synchronously inside Engine::Update() (Observable::Notify calls its subscribers
    // directly on the calling thread), so its cost lands straight on frame time -- keep it
    // proportional to the one new message, never to the number of messages held.
    Console::Get().Subscribe(
        [this](std::any data) {
            if (data.has_value() && data.type() == typeid(std::string))
                this->AddWidget(std::any_cast<std::string>(data));
            return true;
        }, Console::NEW_MESSAGE_EVENT);

    connect(clear_button, &QPushButton::clicked, [](bool) {
        Console::Clear();                
    });

    GenerateWidgets();
}
void ConsoleGui::Init(){
    std::cout << "ConsoleGui Initialized" << std::endl;
}

void ConsoleGui::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
}

void ConsoleGui::GenerateWidgets()
{
    auto& messages = Console::Get().GetMessages();

    bool added = false;
    for (auto& [key, msg] : messages) {
        if (message_widgets.find(key) == message_widgets.end()) {
            MessageGui* gui = new MessageGui(this, msg);
            scrollLayout->addWidget(gui);
            message_widgets.emplace(key, gui);
            added = true;
        }
    }

    // One relayout for the whole batch, not one per widget.
    if (added) {
        scrollLayout->parentWidget()->adjustSize();
        scrollLayout->update();
        content->updateGeometry();
    }
}

void ConsoleGui::AddWidget(const std::string& key)
{
    // Already shown: this was a repeat, and the MessageGui refreshes its own count off the
    // Message's notify. Nothing to build.
    if (message_widgets.find(key) != message_widgets.end())
        return;

    auto& messages = Console::Get().GetMessages();
    auto it = messages.find(key);
    if (it == messages.end())
        return;     // evicted between the notify and here; nothing to show

    MessageGui* gui = new MessageGui(this, it->second);
    scrollLayout->addWidget(gui);
    message_widgets.emplace(key, gui);

    scrollLayout->parentWidget()->adjustSize();
    scrollLayout->update();
    content->updateGeometry();
}