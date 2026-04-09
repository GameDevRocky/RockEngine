#include "component-widgets/ComponentHeader.hpp"
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QStyleOption>
#include <QPointer>
#include "engine/serialization/Registry.hpp"
#include "engine/core/GameObject.hpp"
#include <QHeaderView>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include "engine/components/Component.hpp"

ComponentHeader::ComponentHeader(QWidget* parent) 
    : ComponentHeader("Untitled Label", parent) 
{
}

ComponentHeader::ComponentHeader(std::string label, QWidget* parent) 
    : CollapsableWidget(label, parent) 
{
    this->activeButton->setEnabled(true);
}

void ComponentHeader::Bind(std::string id){
    QPointer<ComponentHeader> safeThis = this;
    Component* comp = Registry::FindInRuntime<Component>(id);
    if (!comp) {
        deleteLater();
        return;
    }

    this->component_id = comp->GetID();
    label->setText(QString::fromStdString(comp->GetTypeName()));
    activeButton->setChecked(comp->GetEnabled());
    connect(activeButton, &QRadioButton::toggled, safeThis, [safeThis](bool val){
        if (!safeThis) return;
        auto* comp = Registry::FindInRuntime<Component>(safeThis->component_id);
        if (comp) {
            comp->SetEnabled(val); 
        }
    }); 
    comp->Subscribe([safeThis](std::any data){
        if (!safeThis) return;
        auto* comp = Registry::FindInRuntime<Component>(safeThis->component_id);
        if (!comp) return;
        bool val = std::any_cast<bool>(data);
        safeThis->activeButton->setChecked(val);
    }, GameObject::ACTIVE_CHANGED_EVENT);
}

void ComponentHeader::paintEvent(QPaintEvent *event) {
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}