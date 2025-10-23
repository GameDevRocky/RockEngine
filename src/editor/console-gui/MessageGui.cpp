#include "MessageGui.hpp"
#include "ConsoleGui.hpp"
#include <QFrame>
#include <QSpacerItem>
#include "editor/utils/EditorUtils.hpp"


constexpr const char* COMMENT_STYLESHEET = R"(
QWidget {
    background-color: #252525;
    color: #EEEEEE;
    border-radius: 6px;
}
QWidget#body{
    border: 1px solid #555;
}
QLabel {
    font-family: Consolas;
    font-size: 14px;
}
QLabel#file_name {
    font-weight: 600;
}
QLabel#countLabel {
    color: #76B47A;
    font-weight: bold;
}
)";

constexpr const char* WARNING_STYLESHEET = R"(
QWidget {
    background-color: #3B2A14;
    color: #FFF3C1;
    border-radius: 6px;
    border: 1px solid #FFA500;
}
QWidget#body{
    border: 1px solid #FFA500;
}
QLabel {
    font-family: Consolas;
    font-size: 14px;
}
QLabel#countLabel {
    color: #FFC400;
    font-weight: bold;
}
    
)";

constexpr const char* ALERT_STYLESHEET = R"(
QWidget {
    background-color: #3A1E1E;
    color: #F5C6CB;
    border-radius: 6px;
}
QWidget#body{
    border: 1px solid #FF5555;
}
QLabel {
    font-family: Consolas;
    font-size: 14px;
}
QLabel#countLabel {
    color: #FF7A7A;
    font-weight: bold;
}
)";

MessageGui::MessageGui(ConsoleGui* parent, Message* msg)
    : QWidget(parent), msg(msg), c_parent(parent) {

    if (msg)
        msg->Subscribe([this]() { Update(); });

    // --- Widget creation ---
    file_path = new EditorUtils::ClickableLabel(this);
    count = new QLabel(this);
    text = new QLabel(this);
    type = new QLabel(this);

    // --- Size policy ---
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    setMinimumHeight(0);
    setMaximumHeight(QWIDGETSIZE_MAX);

    text->setWordWrap(true);
    text->setTextInteractionFlags(Qt::TextSelectableByMouse);
    text->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    file_path->setWordWrap(false);
    file_path->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    file_path->setTextFormat(Qt::RichText);

    type->setAlignment(Qt::AlignCenter);
    type->setStyleSheet("font-weight: bold; text-transform: uppercase;");
    count->setAlignment(Qt::AlignRight);

    // --- Layout ---
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(10, 6, 10, 6);
    main_layout->setSpacing(4);

    auto* header_layout = new QHBoxLayout();
    header_layout->setSpacing(16);
    header_layout->setContentsMargins(0,0,0,0);
    header_layout->addWidget(type);
    header_layout->addWidget(file_path, 1);
    header_layout->addWidget(count);

    auto* divider = new QFrame(this);
    divider->setFrameShape(QFrame::HLine);
    divider->setFrameShadow(QFrame::Sunken);
    divider->setStyleSheet("color: rgba(90,90,90,0.5);");

    main_layout->addLayout(header_layout);
    main_layout->addWidget(divider);
    main_layout->addWidget(text);
    main_layout->addStretch(0); 

    setLayout(main_layout);

    // --- Set stylesheet based on type ---
    if (msg) {
        std::string t = msg->type;
        if (t == "[COMMENT]") setStyleSheet(COMMENT_STYLESHEET);
        else if (t == "[WARNING]") setStyleSheet(WARNING_STYLESHEET);
        else if (t == "[ALERT]") setStyleSheet(ALERT_STYLESHEET);
        else setStyleSheet(COMMENT_STYLESHEET);
    }

    setObjectName("body");
    Update();

    std::string projectRoot = "C:/Users/rockl/Coding Projects/RockEngine/";
    fullPath = projectRoot + msg->file_name;
    file_path->setFilePath(QString::fromStdString(fullPath));

    connect(file_path, &EditorUtils::ClickableLabel::clicked, [this](){
        EditorUtils::OpenInVSCode(file_path->getFilePath().toStdString());
    });

    file_path->setText(QString::fromStdString(msg->file_name));


}


void MessageGui::Update() {
    if (!msg)
        return;

    if (msg->isDestroyed) {
        if (!c_parent) {
            Console::Comment("No Parent Found");
            return;
        }

        for (auto it = c_parent->message_widgets.begin(); it != c_parent->message_widgets.end(); ++it) {
            if (it->second == this) {
                c_parent->message_widgets.erase(it);
                break;
            }
        }
        deleteLater();
        return;
    }

    file_path->setText(QString::fromStdString(msg->file_name));

    count->setText(QString::number(msg->count));
    text->setText(QString::fromStdString(msg->text));
    type->setText(  QString::fromStdString( "[" + msg->type + "]").toUpper());

    adjustSize(); // makes sure height adapts perfectly to content
}
