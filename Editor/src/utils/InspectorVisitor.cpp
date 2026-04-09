#include "utils/InspectorVisitor.hpp"
#include "utils/ProperyFactory.hpp"
#include "engine/core/GameObject.hpp"
#include <QDoubleSpinBox>
#include "engine/components/Transform.hpp"
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
    auto id = transform->GetID();

    auto pos_get = [id](){
        auto* transform = Registry::FindInRuntime<Transform>(id);
        if (!transform) return glm::vec2(0.0f);
        return transform->localPosition;
    };  
    auto pos_set = [id](glm::vec2 pos){
        auto* transform = Registry::FindInRuntime<Transform>(id);
        if (!transform) return;
        transform->SetPosition(pos);
    };  

    auto rot_get = [id](){
        auto* transform = Registry::FindInRuntime<Transform>(id);
        if (!transform) return 0.0f;
        return transform->localRotation;
    };  
    auto rot_set = [id](float val){
        auto* transform = Registry::FindInRuntime<Transform>(id);
        if (!transform) return;
        transform->SetRotation(val);
    }; 
    
    auto scale_get = [id](){
        auto* transform = Registry::FindInRuntime<Transform>(id);
        if (!transform) return glm::vec2(0.0f);
        return transform->localScale;
    };  
    auto scale_set = [id](glm::vec2 pos){
        auto* transform = Registry::FindInRuntime<Transform>(id);
        if (!transform) return;
        transform->SetScale(pos);
    };  
    BindProperty(transform, "Position: ", pos_get, pos_set, transform->POSITION_CHANGED_EVENT, PropDesc().Tag(Tags::VECTOR2).Step(1));
    BindProperty(transform, "Rotation: ", rot_get, rot_set, transform->ROTATION_CHANGED_EVENT, PropDesc().Tag(Tags::ANGLE).Step(1));
    BindProperty(transform, "Scale: ", scale_get, scale_set, transform->SCALE_CHANGED_EVENT, PropDesc().Tag(Tags::VECTOR2).Step(1));
    
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
    spinBoxes[0]->setValue(current.x);
    spinBoxes[1]->setValue(current.y);

    QObject::connect(spinBoxes[0], &QDoubleSpinBox::valueChanged, [=](double val) {
        x->blockSignals(true);
        y->blockSignals(true);
        glm::vec2 updated = getter(); 
        updated.x = (float)val;      
        setter(updated);  
        x->blockSignals(false);
        y->blockSignals(false);     
    });

    QObject::connect(spinBoxes[1], &QDoubleSpinBox::valueChanged, [=](double val) {
        glm::vec2 updated = getter(); 
        updated.y = (float)val;       
        setter(updated);             
    });

    instance->Subscribe([=](){        
        if (x.isNull() || y.isNull()) return;
            glm::vec2 updated = getter();

            x->blockSignals(true);
            y->blockSignals(true);

            x->setValue(updated.x);
            y->setValue(updated.y);

            x->blockSignals(false);
            y->blockSignals(false);
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
        if (spin.isNull()) return;
        float val = getter();

        spin->blockSignals(true);
        spin->setValue(val);
        spin->blockSignals(false);
    }, event_id);

    QLabel* label = new QLabel( text.c_str());
    auto font = label->font(); 
    font.setBold(true);
    label->setFont(font);   
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    hbox->addWidget(label);
    hbox->addStretch();
    hbox->addWidget(spin);
    layout->addLayout(hbox);
    spin->blockSignals(false);
    containsContent = true;
}