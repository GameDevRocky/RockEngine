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

ObjectHeader::ObjectHeader(QWidget* parent) : QWidget(parent)
{
    auto* vbox = new QVBoxLayout(this);
    auto* hbox1 = new QHBoxLayout();
    auto* hbox2 = new QHBoxLayout();



    setLayout(vbox);
    QWidget* header = new QWidget(this);
    header->setLayout(new QHBoxLayout());
    header->layout()->setContentsMargins(0,0,0,0);
    QLabel* headerLabel = new QLabel("Properties");
    QFont font = headerLabel->font(); 
    font.setBold(true);
    font.setPointSize(10);
    headerLabel->setFont(font);
    header->layout()->addWidget(headerLabel);
    vbox->addWidget(header);

    vbox->addLayout(hbox1);
    vbox->addLayout(hbox2);


    activeButton = new QRadioButton(this);
    hbox1->addWidget(activeButton);

    nameEdit = new QLineEdit(this);
    nameEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    hbox1->addWidget(nameEdit);

    tagOptions = new QComboBox(this);
    layerOptions = new QComboBox(this);

    tagOptions->addItems({"Player", "Car", "Coin"});
    layerOptions->addItems({"Default", "FX", "Particle", "UI"});

    tagOptions->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    layerOptions->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    auto* tagLabel = new QLabel("Tag: ");
    auto* layerLabel = new QLabel("Layer: ");

    tagLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    layerLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);

    hbox2->addWidget(tagLabel);
    hbox2->addWidget(tagOptions);

    hbox2->addWidget(layerLabel);
    hbox2->addWidget(layerOptions);
}

void ObjectHeader::paintEvent(QPaintEvent *event) {
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void ObjectHeader::Bind(const std::string& id) {
    auto* obj = Registry::FindInRuntime<GameObject>(id);
    if (!obj) {
        deleteLater();
        return;
    }
    
    this->gameobject_id = id;
    nameEdit->setText(QString::fromStdString(obj->GetName()));
    activeButton->setChecked(obj->GetActive());

    connect(nameEdit, &QLineEdit::textEdited, this, [this](const QString &name){
        auto* currentObj = Registry::FindInRuntime<GameObject>(this->gameobject_id);
        if (currentObj) {
            currentObj->SetName(name.toStdString()); 
        }
    }); 

    connect(activeButton, &QRadioButton::toggled, this, [this](bool val){
        auto* currentObj = Registry::FindInRuntime<GameObject>(this->gameobject_id);
        if (currentObj) {
            currentObj->SetActive(val); 
        }
    }); 

    QPointer<ObjectHeader> safeThis = this;

    obj->Subscribe([safeThis](std::any data){
        if (!safeThis) return;
        auto* currentObj = Registry::FindInRuntime<GameObject>(safeThis->gameobject_id);
        if (!currentObj) return; 
        std::string name = std::any_cast<std::string>(data);
        safeThis->nameEdit->setText(QString::fromStdString(name));
    }, GameObject::NAME_CHANGED_EVENT);

    obj->Subscribe([safeThis](std::any data){
        if (!safeThis) return;
        auto* currentObj = Registry::FindInRuntime<GameObject>(safeThis->gameobject_id);
        if (!currentObj) return;
        bool val = std::any_cast<bool>(data);
        safeThis->activeButton->setChecked(val);
    }, GameObject::ACTIVE_CHANGED_EVENT);
}