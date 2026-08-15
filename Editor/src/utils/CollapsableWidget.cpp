#include "utils/CollapsableWidget.hpp"
#include <QHBoxLayout>
#include <QPainter>
#include <QStyleOption>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include "utils/EditorUtils.hpp"

CollapsableWidget::CollapsableWidget(std::string label, QWidget* parent) : QWidget(parent) 
{
    header = new QWidget(this);
    header->setObjectName("collapsableHeader");
    header->setAttribute(Qt::WA_StyledBackground, true);

    QHBoxLayout* headerLayout = new QHBoxLayout();
    header->setLayout(headerLayout);
    headerLayout->setContentsMargins(5, 0, 7, 0);
    headerLayout->setSpacing(2);

    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    toggleButton = new QToolButton(this);
    toggleButton->setObjectName("CollapseToggleButton");
    toggleButton->setFixedSize(24,24);
    toggleButton->setIconSize(QSize(24, 24));
    toggleButton->setCheckable(true);
    toggleButton->setChecked(true);
    toggleButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toggleButton->setArrowType(Qt::NoArrow);
    toggleButton->setIcon(EditorUtils::CustomIconProvider::disclosureArrowDownIcon());
    toggleButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    iconButton = new QPushButton();
    iconButton->setFlat(true);
    iconButton->setEnabled(false);
    iconButton->hide();

    activeButton = new QCheckBox();
    activeButton->setObjectName("CollapsableActiveToggle");
    activeButton->setAutoFillBackground(false);
    activeButton->setFixedSize(20, 20);
    activeButton->setEnabled(false);
    activeButton->setCheckable(true);
    activeButton->setChecked(false);
    //activeButton->hide();

    this->label = new QLineEdit(label.c_str());
    this->label->setObjectName("Label");
    this->label->setReadOnly(true);
    QFont font = this->label->font();
    font.setBold(true);
    this->label->setFont(font);
  
    contentWidget = new QWidget(this);
    contentLayout = new QVBoxLayout(contentWidget);

    collapseAnim = new QPropertyAnimation(contentWidget, "maximumHeight", this);
    collapseAnim->setDuration(180);
    collapseAnim->setEasingCurve(QEasingCurve::InOutCubic);

    connect(toggleButton, &QToolButton::toggled, this, [this](bool checked) {
        toggleButton->setIcon(checked
            ? EditorUtils::CustomIconProvider::disclosureArrowDownIcon()
            : EditorUtils::CustomIconProvider::disclosureArrowRightIcon());
        collapseAnim->stop();
        if (checked) {
            contentWidget->setVisible(true);   // must be visible while it grows
            collapseAnim->setStartValue(0);
            collapseAnim->setEndValue(contentWidget->sizeHint().height());
        } else {
            collapseAnim->setStartValue(contentWidget->height());
            collapseAnim->setEndValue(0);
        }
        collapseAnim->start();
    });

    connect(collapseAnim, &QPropertyAnimation::finished, this, [this]() {
        if (toggleButton->isChecked())
            contentWidget->setMaximumHeight(QWIDGETSIZE_MAX);  // release cap so it grows with content
        else
            contentWidget->setVisible(false);                  // drop it from the layout when closed
    });

    connect(activeButton, &QCheckBox::toggled,[this](bool checked){
        OnActiveToggled(checked);
    });

    headerLayout->addWidget(toggleButton);
    headerLayout->addWidget(iconButton);
    headerLayout->addWidget(activeButton);
    headerLayout->addWidget(this->label);

    // Pin the header to its natural height so it never stretches vertically.
    header->setFixedHeight(header->sizeHint().height());

    mainLayout->addWidget(header);
    mainLayout->addWidget(contentWidget);
}

void CollapsableWidget::AddWidget(QWidget* widget) {
    contentLayout->addWidget(widget);
}

void CollapsableWidget::paintEvent(QPaintEvent *event) {
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}
