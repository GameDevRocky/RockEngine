#include "utils/CollapsableWidget.hpp"
#include <QHBoxLayout>
#include <QPainter>
#include <QStyleOption>

CollapsableWidget::CollapsableWidget(std::string label, QWidget* parent) : QWidget(parent) 
{
    header = new QWidget(this);
    header->setStyleSheet("{border: 1px solid rgb(67, 67, 67);}");

    QHBoxLayout* headerLayout = new QHBoxLayout();
    header->setLayout(headerLayout);
    headerLayout->setContentsMargins(4, 8, 4, 8);
    headerLayout->setSpacing(4);

    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    toggleButton = new QToolButton(this);
    toggleButton->setFixedSize(24,24);
    toggleButton->setIconSize(QSize(24, 24));
    toggleButton->setCheckable(true);
    toggleButton->setChecked(true);
    toggleButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toggleButton->setArrowType(Qt::DownArrow);
    toggleButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    iconButton = new QPushButton();
    iconButton->setFlat(true);
    iconButton->setEnabled(false);
    iconButton->hide();

    activeButton = new QCheckBox();
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

    connect(toggleButton, &QToolButton::toggled, [this](bool checked) {
        toggleButton->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
        contentWidget->setVisible(checked);
    });

    connect(activeButton, &QCheckBox::toggled,[this](bool checked){
        OnActiveToggled(checked);
    });

    headerLayout->addWidget(toggleButton);
    headerLayout->addWidget(iconButton);
    headerLayout->addWidget(activeButton);
    headerLayout->addWidget(this->label);
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