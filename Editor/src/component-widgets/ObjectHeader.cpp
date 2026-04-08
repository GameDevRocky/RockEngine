#include "component-widgets/ObjectHeader.hpp"
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


ObjectHeader::ObjectHeader(QWidget* parent) 
    : ObjectHeader("Untitled Label", parent) 
{
}

ObjectHeader::ObjectHeader(std::string label, QWidget* parent) 
    : CollapsableWidget(label, parent) 
{
    this->label->setReadOnly(false);
    this->activeButton->setEnabled(true);
    QFormLayout *formLayout = new QFormLayout();
    formLayout->addRow(new QLabel("Object Name:"), new QLineEdit());
    formLayout->addRow(new QLabel("Physics Scale:"), new QDoubleSpinBox());
    formLayout->addRow(new QLabel("Is Static:"), new QCheckBox());
    formLayout->setLabelAlignment(Qt::AlignLeft);
    this->contentLayout->addLayout(formLayout);


}
void ObjectHeader::paintEvent(QPaintEvent *event) {
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void ObjectHeader::Bind(const std::string id) {
    QPointer<ObjectHeader> safeThis = this;
    auto* obj = Registry::FindInRuntime<GameObject>(id);
    if (!obj) {
        deleteLater();
        return;
    }
    
    this->gameobject_id = id;
    label->setText(QString::fromStdString(obj->GetName()));
    activeButton->setChecked(obj->GetActive());

    connect(activeButton, &QRadioButton::toggled, safeThis, [safeThis](bool val){
        if (!safeThis) return;
        auto* currentObj = Registry::FindInRuntime<GameObject>(safeThis->gameobject_id);
        if (currentObj) {
            currentObj->SetActive(val); 
        }
    }); 

    connect(label, &QLineEdit::editingFinished, safeThis, [safeThis](){
        if (!safeThis) return;
        auto* currentObj = Registry::FindInRuntime<GameObject>(safeThis->gameobject_id);
        if (currentObj) {
            currentObj->SetName(safeThis->label->text().toStdString()); 
        }
    }); 

    obj->Subscribe([safeThis](std::any data){
        if (!safeThis) return;
        auto* currentObj = Registry::FindInRuntime<GameObject>(safeThis->gameobject_id);
        if (!currentObj) return; 
        std::string name = std::any_cast<std::string>(data);
        safeThis->label->setText(QString::fromStdString(name));
    }, GameObject::NAME_CHANGED_EVENT);

    obj->Subscribe([safeThis](std::any data){
        if (!safeThis) return;
        auto* currentObj = Registry::FindInRuntime<GameObject>(safeThis->gameobject_id);
        if (!currentObj) return;
        bool val = std::any_cast<bool>(data);
        safeThis->activeButton->setChecked(val);
    }, GameObject::ACTIVE_CHANGED_EVENT);
}