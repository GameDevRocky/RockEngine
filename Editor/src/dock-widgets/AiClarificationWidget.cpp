#include "dock-widgets/AiClarificationWidget.hpp"

#include "utils/EditorUtils.hpp"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QStyle>
#include <QToolButton>
#include <QVariant>
#include <QVBoxLayout>

namespace {

QStringList SelectedIds(const QJsonObject& answer) {
    QStringList result;
    for (const QJsonValue& value : answer.value(QStringLiteral("selectedIds")).toArray())
        result.push_back(value.toString());
    return result;
}

} // namespace

AiClarificationWidget::AiClarificationWidget(const QJsonObject& request,
                                             const QString& status,
                                             const QJsonObject& answer,
                                             bool expanded,
                                             QWidget* parent)
    : QWidget(parent),
      m_pending(status == QStringLiteral("pending")),
      m_allowMultiple(request.value(QStringLiteral("allowMultiple")).toBool()) {
    setObjectName(QStringLiteral("AiClarificationBubble"));
    setAttribute(Qt::WA_StyledBackground, true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(13, 11, 13, 12);
    root->setSpacing(9);

    auto* header = new QWidget(this);
    header->setObjectName(QStringLiteral("AiClarificationHeader"));
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(7);

    QToolButton* disclosure = nullptr;
    if (!m_pending) {
        disclosure = new QToolButton(header);
        disclosure->setObjectName(QStringLiteral("AiClarificationDisclosure"));
        disclosure->setArrowType(Qt::NoArrow);
        disclosure->setIcon(expanded
            ? EditorUtils::CustomIconProvider::disclosureArrowDownIcon()
            : EditorUtils::CustomIconProvider::disclosureArrowRightIcon());
        disclosure->setIconSize(QSize(13, 13));
        disclosure->setCheckable(true);
        disclosure->setChecked(expanded);
        disclosure->setCursor(Qt::PointingHandCursor);
        disclosure->setToolTip(expanded
            ? QStringLiteral("Collapse clarification")
            : QStringLiteral("Show clarification"));
        headerLayout->addWidget(disclosure, 0, Qt::AlignTop);
    }

    auto* headerText = new QWidget(header);
    headerText->setObjectName(QStringLiteral("AiClarificationHeaderText"));
    auto* headerTextLayout = new QVBoxLayout(headerText);
    headerTextLayout->setContentsMargins(0, 0, 0, 0);
    headerTextLayout->setSpacing(1);
    auto* title = new QLabel(
        request.value(QStringLiteral("title")).toString(QStringLiteral("Clarification needed")),
        headerText);
    title->setObjectName(QStringLiteral("AiClarificationTitle"));
    title->setWordWrap(true);
    headerTextLayout->addWidget(title);

    auto* summary = new QLabel(
        m_pending ? QStringLiteral("Your input is needed before the assistant can continue.")
                  : AnswerSummary(status, answer),
        headerText);
    summary->setObjectName(QStringLiteral("AiClarificationSummary"));
    summary->setWordWrap(true);
    headerTextLayout->addWidget(summary);
    headerLayout->addWidget(headerText, 1);
    if (disclosure) {
        const auto makeHeaderClickable = [this, disclosure](QWidget* target) {
            target->setProperty(
                "clarificationDisclosure", QVariant::fromValue<QObject*>(disclosure));
            target->setCursor(Qt::PointingHandCursor);
            target->setToolTip(QStringLiteral("Show or hide clarification details"));
            target->installEventFilter(this);
        };
        makeHeaderClickable(header);
        makeHeaderClickable(headerText);
        makeHeaderClickable(title);
        makeHeaderClickable(summary);
    }
    root->addWidget(header);

    auto* body = new QWidget(this);
    body->setObjectName(QStringLiteral("AiClarificationBody"));
    body->setAttribute(Qt::WA_StyledBackground, true);
    auto* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(0, 3, 0, 0);
    bodyLayout->setSpacing(8);

    auto* question = new QLabel(request.value(QStringLiteral("question")).toString(), body);
    question->setObjectName(QStringLiteral("AiClarificationQuestion"));
    question->setTextFormat(Qt::PlainText);
    question->setTextInteractionFlags(Qt::TextSelectableByMouse);
    question->setWordWrap(true);
    bodyLayout->addWidget(question);

    const QString contextText = request.value(QStringLiteral("context")).toString();
    if (!contextText.isEmpty()) {
        auto* context = new QLabel(contextText, body);
        context->setObjectName(QStringLiteral("AiClarificationContext"));
        context->setTextFormat(Qt::PlainText);
        context->setTextInteractionFlags(Qt::TextSelectableByMouse);
        context->setWordWrap(true);
        bodyLayout->addWidget(context);
    }

    auto* instruction = new QLabel(
        m_allowMultiple ? QStringLiteral("Select all that apply:")
                        : QStringLiteral("Choose the next action:"), body);
    instruction->setObjectName(QStringLiteral("AiClarificationInstruction"));
    bodyLayout->addWidget(instruction);

    auto* options = new QWidget(body);
    options->setObjectName(QStringLiteral("AiClarificationOptions"));
    auto* optionsLayout = new QVBoxLayout(options);
    optionsLayout->setContentsMargins(0, 0, 0, 0);
    optionsLayout->setSpacing(5);
    if (!m_allowMultiple) m_group = new QButtonGroup(this);
    const QStringList selectedIds = SelectedIds(answer);
    for (const QJsonValue& value : request.value(QStringLiteral("options")).toArray())
        AddOption(optionsLayout, value.toObject(), selectedIds);
    bodyLayout->addWidget(options);

    m_otherText = new QPlainTextEdit(body);
    m_otherText->setObjectName(QStringLiteral("AiClarificationOtherText"));
    m_otherText->setPlaceholderText(
        QStringLiteral("Describe another option or provide more context…"));
    m_otherText->setMaximumHeight(72);
    m_otherText->setPlainText(answer.value(QStringLiteral("otherText")).toString());
    const bool otherSelected = selectedIds.contains(QStringLiteral("other"));
    if (m_pending) {
        m_otherText->setEnabled(otherSelected);
    } else {
        m_otherText->setReadOnly(true);
        m_otherText->setVisible(otherSelected);
    }
    bodyLayout->addWidget(m_otherText);

    if (m_pending) {
        auto* actions = new QHBoxLayout();
        actions->setContentsMargins(0, 2, 0, 0);
        actions->setSpacing(6);
        actions->addStretch(1);
        auto* cancel = new QPushButton(QStringLiteral("Cancel"), body);
        cancel->setObjectName(QStringLiteral("AiClarificationCancel"));
        m_continue = new QPushButton(QStringLiteral("Continue"), body);
        m_continue->setObjectName(QStringLiteral("AiClarificationContinue"));
        m_continue->setEnabled(false);
        actions->addWidget(cancel);
        actions->addWidget(m_continue);
        bodyLayout->addLayout(actions);

        connect(cancel, &QPushButton::clicked,
                this, &AiClarificationWidget::CancelRequested);
        connect(m_continue, &QPushButton::clicked, this, [this]() {
            QStringList selected;
            for (QAbstractButton* choice : m_choices) {
                if (choice->isChecked())
                    selected.push_back(choice->property("clarificationId").toString());
            }
            Q_EMIT AnswerSubmitted(selected, m_otherText->toPlainText().trimmed());
        });
        connect(m_otherText, &QPlainTextEdit::textChanged,
                this, &AiClarificationWidget::RefreshContinue);
        RefreshContinue();
    }

    body->setVisible(m_pending || expanded);
    root->addWidget(body);
    if (disclosure) {
        connect(disclosure, &QToolButton::toggled, this,
                [this, disclosure, body](bool isExpanded) {
            disclosure->setIcon(isExpanded
                ? EditorUtils::CustomIconProvider::disclosureArrowDownIcon()
                : EditorUtils::CustomIconProvider::disclosureArrowRightIcon());
            disclosure->setToolTip(isExpanded
                ? QStringLiteral("Collapse clarification")
                : QStringLiteral("Show clarification"));
            body->setVisible(isExpanded);
            Q_EMIT ExpansionChanged(isExpanded);
        });
    }
}

void AiClarificationWidget::AddOption(QVBoxLayout* layout,
                                      const QJsonObject& option,
                                      const QStringList& selectedIds) {
    auto* card = new QFrame(this);
    card->setObjectName(QStringLiteral("AiClarificationOption"));
    card->setAttribute(Qt::WA_StyledBackground, true);
    auto* row = new QHBoxLayout(card);
    row->setContentsMargins(9, 7, 9, 7);
    row->setSpacing(8);

    QAbstractButton* choice = m_allowMultiple
        ? static_cast<QAbstractButton*>(new QCheckBox(card))
        : static_cast<QAbstractButton*>(new QRadioButton(card));
    const QString id = option.value(QStringLiteral("id")).toString();
    choice->setObjectName(QStringLiteral("AiClarificationChoice"));
    choice->setProperty("clarificationId", id);
    if (m_group) m_group->addButton(choice);
    choice->setChecked(selectedIds.contains(id));
    choice->setEnabled(m_pending);
    m_choices.push_back(choice);
    row->addWidget(choice, 0, Qt::AlignVCenter);

    auto* text = new QWidget(card);
    text->setObjectName(QStringLiteral("AiClarificationOptionText"));
    auto* textLayout = new QVBoxLayout(text);
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(1);
    auto* label = new QLabel(option.value(QStringLiteral("label")).toString(), text);
    label->setObjectName(QStringLiteral("AiClarificationOptionLabel"));
    label->setWordWrap(true);
    textLayout->addWidget(label);
    QLabel* description = nullptr;
    const QString descriptionText = option.value(QStringLiteral("description")).toString();
    if (!descriptionText.isEmpty()) {
        description = new QLabel(descriptionText, text);
        description->setObjectName(QStringLiteral("AiClarificationOptionDescription"));
        description->setTextFormat(Qt::PlainText);
        description->setWordWrap(true);
        textLayout->addWidget(description);
    }
    row->addWidget(text, 1);

    const auto makeCardClickable = [this, choice](QObject* target) {
        target->setProperty("clarificationChoice", QVariant::fromValue<QObject*>(choice));
        target->installEventFilter(this);
    };
    makeCardClickable(card);
    makeCardClickable(text);
    makeCardClickable(label);
    if (description) makeCardClickable(description);

    connect(choice, &QAbstractButton::toggled, this, [this, card, choice](bool checked) {
        RefreshOptionStyle(card, checked);
        if (choice->property("clarificationId").toString() == QStringLiteral("other") &&
            m_otherText) {
            m_otherText->setEnabled(checked);
        }
        RefreshContinue();
    });
    RefreshOptionStyle(card, choice->isChecked());
    layout->addWidget(card);
}

bool AiClarificationWidget::eventFilter(QObject* watched, QEvent* event) {
    const bool leftRelease = event->type() == QEvent::MouseButtonRelease &&
        static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton;
    if (!m_pending && leftRelease) {
        QObject* object = watched->property("clarificationDisclosure").value<QObject*>();
        if (auto* disclosure = qobject_cast<QAbstractButton*>(object)) {
            disclosure->click();
            return true;
        }
    }
    if (m_pending && leftRelease) {
        QObject* object = watched->property("clarificationChoice").value<QObject*>();
        if (auto* choice = qobject_cast<QAbstractButton*>(object); choice && choice->isEnabled()) {
            if (m_allowMultiple) choice->setChecked(!choice->isChecked());
            else choice->setChecked(true);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void AiClarificationWidget::RefreshContinue() {
    if (!m_continue || !m_otherText) return;
    bool hasSelection = false;
    bool otherNeedsText = false;
    for (QAbstractButton* choice : m_choices) {
        if (!choice->isChecked()) continue;
        hasSelection = true;
        if (choice->property("clarificationId").toString() == QStringLiteral("other"))
            otherNeedsText = m_otherText->toPlainText().trimmed().isEmpty();
    }
    m_continue->setEnabled(hasSelection && !otherNeedsText);
}

void AiClarificationWidget::RefreshOptionStyle(QWidget* card, bool selected) {
    card->setProperty("selected", selected);
    card->style()->unpolish(card);
    card->style()->polish(card);
    card->update();
}

QString AiClarificationWidget::AnswerSummary(const QString& status,
                                             const QJsonObject& answer) const {
    if (status == QStringLiteral("cancelled"))
        return QStringLiteral("Cancelled — no changes were made.");

    QStringList labels;
    for (const QJsonValue& value : answer.value(QStringLiteral("selected")).toArray())
        labels.push_back(value.toObject().value(QStringLiteral("label")).toString());
    QString summary = labels.isEmpty()
        ? QStringLiteral("Answered")
        : QStringLiteral("Answered: %1").arg(labels.join(QStringLiteral(", ")));
    const QString other = answer.value(QStringLiteral("otherText")).toString().trimmed();
    if (!other.isEmpty()) summary += QStringLiteral(" — %1").arg(other);
    return summary;
}
