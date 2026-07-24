#include "utils/MessageGui.hpp"
#include "dock-widgets/ConsoleGui.hpp"
#include <QFrame>
#include "utils/EditorUtils.hpp"
#include "engine/utils/EngineUtils.hpp"

// Dark Theme Colors
static constexpr const char* FONT_FAMILY = "Consolas";
static constexpr int FONT_SIZE = 13;

// Lazy-initialized colors (to avoid static initialization before QApplication)
static QColor* BG_DARK = nullptr;
static QColor* FG_TEXT = nullptr;

static QColor* BORDER_COMMENT = nullptr;
static QColor* BORDER_WARNING = nullptr;
static QColor* BORDER_ALERT = nullptr;

static void InitializeColors() {
    if (!BG_DARK) {
        BG_DARK = new QColor(25, 25, 25);
        FG_TEXT = new QColor(230, 230, 230);
        BORDER_COMMENT = new QColor(90, 90, 90, 125);
        BORDER_WARNING = new QColor(255, 150, 50, 125);
        BORDER_ALERT = new QColor(255, 80, 80, 125);
    }
}

//
// Apply style ONLY to the outermost widget
//
static void ApplyOuterBorder(QWidget* target, const std::string& type)
{
    InitializeColors();
    QColor border;

    if (type == "warning") border = *BORDER_WARNING;
    else if (type == "alert") border = *BORDER_ALERT;
    else border = *BORDER_COMMENT;  // default/comment

    QString style = QString(R"(
    QWidget#MessageRoot {
        background-color: %1;
        border-radius: 6px;
        border: 1px solid %2;
    }
    )")
    .arg(BG_DARK->name())
    .arg(border.name(QColor::HexArgb));  // include alpha


    target->setStyleSheet(style);
}

//
// Apply clean inner stylesheet (no border, transparent children)
//
static void ApplyInnerStyle(QWidget* target)
{
    InitializeColors();
    QString style = QString(R"(
        QWidget {
            background-color: transparent;
            color: %1;
            border: none;
        }
        QLabel {
            font-family: %2;
            font-size: %3px;
            background-color: transparent;
            border: none;
        }
    )")
    .arg(FG_TEXT->name())
    .arg(FONT_FAMILY)
    .arg(FONT_SIZE);

    target->setStyleSheet(style);
}


// ======================================================================
//  MessageGui Implementation
// ======================================================================

MessageGui::MessageGui(ConsoleGui* parent, Message* msg)
    : QWidget(parent), msg(msg), c_parent(parent)
{
    setObjectName("MessageRoot");   // Important: ONLY the outer widget gets the border

    if (msg)
        msg->Subscribe([this]() { 
            Update(); 
            return true;
        });

    // Widgets
    file_path = new EditorUtils::ClickableLabel(this);
    count     = new QLabel(this);
    text      = new QLabel(this);
    type      = new QLabel(this);

    // Word wrap and interaction
    text->setWordWrap(true);
    text->setTextInteractionFlags(Qt::TextSelectableByMouse);

    type->setAlignment(Qt::AlignCenter);
    type->setStyleSheet("font-weight: bold;");

    count->setAlignment(Qt::AlignRight);

    // Outer layout
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(12, 10, 12, 10);
    main_layout->setSpacing(6);

    // Header row
    auto* header_layout = new QHBoxLayout();
    header_layout->setSpacing(8);
    header_layout->setContentsMargins(0,0,0,0);

    header_layout->addWidget(type);
    header_layout->addWidget(file_path, 1);
    header_layout->addWidget(count);

    // Divider
    auto* divider = new QFrame(this);
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet("color: #444;");

    // Add items
    main_layout->addLayout(header_layout);
    main_layout->addWidget(divider);
    main_layout->addWidget(text);

    //
    // Apply styles
    //
    ApplyOuterBorder(this, msg->type);      // ONLY OUTER BORDER
    ApplyInnerStyle(this->findChild<QWidget*>()); // All children borderless

    Update();

    // Set path
    fullPath = EngineUtils::GetAssetPath(msg->file_name) + ":" + msg->line_num;
    file_path->setFilePath(QString::fromStdString(fullPath));

    connect(file_path, &EditorUtils::ClickableLabel::clicked, [this]() {
        EditorUtils::OpenInVSCode(file_path->getFilePath().toStdString());
    });
}


// ======================================================================
// Update
// ======================================================================

void MessageGui::Update()
{
    if (!msg)
        return;

    // Handle deletion
    if (msg->isDestroyed)
    {
        if (!c_parent) return;

        for (auto it = c_parent->message_widgets.begin(); it != c_parent->message_widgets.end(); ++it)
        {
            if (it->second == this) {
                c_parent->message_widgets.erase(it);
                break;
            }
        }

        deleteLater();
        return;
    }

    // Update contents
    file_path->setText(QString::fromStdString(msg->file_name));
    count->setText(QString::number(msg->count));
    text->setText(QString::fromStdString(msg->text));
    type->setText("[" + QString::fromStdString(msg->type).toUpper() + "]");

    // Re-apply outer border only if type changed
    ApplyOuterBorder(this, msg->type);

    adjustSize();
}
