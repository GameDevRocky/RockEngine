#include "utils/InspectorVisitor.hpp"
#include "utils/ProperyFactory.hpp"
#include "engine/core/GameObject.hpp"
#include <QDoubleSpinBox>
#include "engine/components/Transform.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include "engine/utils/Properties.hpp"
#include <QPointer>
#include <Qt>

using namespace Properties;

InspectorVisitor::InspectorVisitor(){
    content = new QWidget();
    layout = new QVBoxLayout();
    layout->setContentsMargins(0,0,0,0);
    content->setLayout(layout);
}


void InspectorVisitor::Visit(GameObject* obj){
    
    
}

void InspectorVisitor::Visit(Transform* transform){

    auto pos_get = [=](){
        if (!transform) return glm::vec2(0.0f);
        return transform->localPosition;
    };  

    auto pos_set = [=](glm::vec2 pos){
        if (!transform) return;
        transform->SetPosition(pos);
    };  

    auto rot_get = [=]() -> float {
        if (!transform) return 0.0f;
        return transform->localRotation;
    };  

    auto rot_set = [=](float val){
        if (!transform) return;
        transform->SetRotation(val);
    }; 
    
    auto scale_get = [=](){
        if (!transform) return glm::vec2(0.0f);
        return transform->localScale;
    };  

    auto scale_set = [=](glm::vec2 pos){
        if (!transform) return;
        transform->SetScale(pos);
    };  

    BindProperty(transform, "Position: ", pos_get, pos_set, transform->POSITION_CHANGED_EVENT, PropDesc().Tag(Tags::VECTOR2).Step(1));
    BindProperty(transform, "Rotation: ", rot_get, rot_set, transform->ROTATION_CHANGED_EVENT, PropDesc().Tag(Tags::ANGLE).Step(1));
    BindProperty(transform, "Scale: ", scale_get, scale_set, transform->SCALE_CHANGED_EVENT, PropDesc().Tag(Tags::VECTOR2).Step(1));
    
    
}

void InspectorVisitor::Visit(SpriteRenderer* renderer){

    auto color_get = [=](){
        return renderer->GetColor();
    };
    auto color_set = [=](glm::vec4 color){
        renderer->SetColor(color);
    };

    auto flipX_get = [=]() -> bool {
        return renderer->GetFlipX();
    };
    auto flipX_set = [=](bool val){
        renderer->SetFlipX(val);
    };

    BindProperty(renderer, "Color: ", color_get, color_set, renderer->COLOR_CHANGED_EVENT, PropDesc().Tag(Tags::COLOR));
    BindToggleProperty(renderer, "Flip X: ", flipX_get, flipX_set, renderer->CHANGED_EVENT, PropDesc().Tag(Tags::TOGGLE));
}

void InspectorVisitor::BindProperty(Serializable* instance, const std::string& text, std::function<glm::vec2()> getter,
            std::function<void(glm::vec2)> setter, Observable::Event event_id, PropDesc desc)
{
    std::string id = instance->GetID();
    QHBoxLayout* hbox = new QHBoxLayout();
    QWidget* widget = PropertyFactory::Create(desc.tag, desc);
    auto spinBoxes = widget->findChildren<QDoubleSpinBox*>();
    QPointer<QDoubleSpinBox> x = spinBoxes[0];
    QPointer<QDoubleSpinBox> y = spinBoxes[1];
    x->blockSignals(true);
    y->blockSignals(true);

    glm::vec2 current = getter(); 
    x->setValue(current.x);
    y->setValue(current.y);

    QObject::connect(x, &QDoubleSpinBox::valueChanged, [=](double val) {
        glm::vec2 updated = getter(); 
        updated.x = (float)val;      
        setter(updated);    
    });

    QObject::connect(y, &QDoubleSpinBox::valueChanged, [=](double val) {
        glm::vec2 updated = getter(); 
        updated.y = (float)val;       
        setter(updated);             
    });

    instance->Subscribe([=](){        
        if (x.isNull() || y.isNull()) return false;
             glm::vec2 updated = getter();

            x->blockSignals(true);
            y->blockSignals(true);

            x->setValue(updated.x);
            y->setValue(updated.y);

            x->blockSignals(false);
            y->blockSignals(false);
            return true;
    }, event_id);

    QLabel* label = new QLabel( text.c_str());
    auto font = label->font(); 
    font.setBold(true);
    label->setFont(font);   
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    hbox->addWidget(label);
    hbox->addStretch();
    hbox->addWidget(widget);
    layout->addLayout(hbox);
    x->blockSignals(false);
    y->blockSignals(false);
    containsContent = true;
}



void InspectorVisitor::BindProperty(Serializable* instance, const std::string& text, std::function<float()> getter,
            std::function<void(float)> setter, Observable::Event event_id, PropDesc desc)
{
    std::string id = instance->GetID();
    QHBoxLayout* hbox = new QHBoxLayout();
    QPointer<QDoubleSpinBox> spin = static_cast<QDoubleSpinBox*>(PropertyFactory::Create(desc.tag, desc));
    spin->blockSignals(true);     

    float current = getter(); 
    spin->setValue(current);

    QObject::connect(spin, &QDoubleSpinBox::valueChanged, [=](double val) {
        spin->blockSignals(true);
        setter(val);  
        spin->blockSignals(false);     
    });

    instance->Subscribe([=](){        
        if (spin.isNull()) return false;
        float val = getter();

        spin->blockSignals(true);
        spin->setValue(val);
        spin->blockSignals(false);
        return true;
    }, event_id);

    QLabel* label = new QLabel( text.c_str());
    auto font = label->font(); 
    font.setBold(true);
    label->setFont(font);   
    //label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    hbox->addWidget(label);
    hbox->addStretch();
    hbox->addWidget(spin);
    layout->addLayout(hbox);
    spin->blockSignals(false);
    containsContent = true;
}


void InspectorVisitor::BindProperty(Serializable* instance, const std::string& text, 
                                    std::function<glm::vec4()> getter,
                                    std::function<void(glm::vec4)> setter, 
                                    Observable::Event event_id, PropDesc desc)
{
    QHBoxLayout* hbox = new QHBoxLayout();
    QPushButton* colorBtn = static_cast<QPushButton*>(PropertyFactory::Create(desc.tag, desc));
    colorBtn->setObjectName("ColorButton");
    QPointer<QPushButton> safeBtn = colorBtn;
    auto updateBtnUI = [safeBtn](glm::vec4 color) {
        if (safeBtn.isNull()) return;
        QColor qcol(color.r * 255, color.g * 255, color.b * 255, color.a * 255);
        safeBtn->setStyleSheet(QString("background-color: %1;").arg(qcol.name()));
    };

    updateBtnUI(getter());
    QObject::connect(colorBtn, &QPushButton::clicked, [=]() {
        glm::vec4 current = getter();
        QColor initial(current.r * 255, current.g * 255, current.b * 255, current.a * 255);
        QColorDialog* dialog = new QColorDialog(); 
        dialog->setCurrentColor(initial);
        dialog->setOptions(QColorDialog::ShowAlphaChannel);
        dialog->setModal(false);
        dialog->setAttribute(Qt::WA_DeleteOnClose); 

        QObject::connect(dialog, &QColorDialog::currentColorChanged, [=](const QColor& selected) {
            if (selected.isValid()) {
                glm::vec4 engineColor(
                    selected.redF(), 
                    selected.greenF(), 
                    selected.blueF(), 
                    selected.alphaF()
                );
                setter(engineColor);   
                updateBtnUI(engineColor); 
            }
        });
        dialog->setWindowFlags(Qt::Popup);
        dialog->show(); 
    });

    instance->Subscribe([=](){         
        if (safeBtn.isNull()) return false;
        updateBtnUI(getter());
        return true;
    }, event_id);

    QLabel* label = new QLabel(text.c_str());
    auto font = label->font(); 
    font.setBold(true);
    label->setFont(font);   
    label->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    colorBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    hbox->addWidget(label);
    hbox->addWidget(colorBtn);
    layout->addLayout(hbox);
    
    containsContent = true;
}

void InspectorVisitor::BindToggleProperty(Serializable* instance, const std::string& text, std::function<bool()> getter,
            std::function<void(bool)> setter, Observable::Event event_id, PropDesc desc)
{
    QHBoxLayout* hbox = new QHBoxLayout();
    QCheckBox* checkbox = static_cast<QCheckBox*>(PropertyFactory::Create(desc.tag, desc));
    QPointer<QCheckBox> safeBox = checkbox;

    checkbox->blockSignals(true);
    checkbox->setChecked(getter());
    checkbox->blockSignals(false);

    QObject::connect(checkbox, &QCheckBox::toggled, [=](bool val) {
        setter(val);
    });

    instance->Subscribe([=]() {
        if (safeBox.isNull()) return false;
        safeBox->blockSignals(true);
        safeBox->setChecked(getter());
        safeBox->blockSignals(false);
        return true;
    }, event_id);

    QLabel* label = new QLabel(text.c_str());
    auto font = label->font();
    font.setBold(true);
    label->setFont(font);
    label->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    hbox->addWidget(label);
    hbox->addStretch();
    hbox->addWidget(checkbox);
    layout->addLayout(hbox);

    containsContent = true;
}