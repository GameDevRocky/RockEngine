#include "utils/InspectorVisitor.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/components/Transform.hpp"
#include "engine/components/SpriteRenderer.hpp"

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

    auto rot_get = [=](){
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

    BindProperty<glm::vec2>(transform, "Position: ", pos_get, pos_set, transform->POSITION_CHANGED_EVENT, PropDesc().Tag(Tags::VECTOR2).Step(1));
    BindProperty<float>(transform, "Rotation: ", rot_get, rot_set, transform->ROTATION_CHANGED_EVENT, PropDesc().Tag(Tags::ANGLE).Step(1));
    BindProperty<glm::vec2>(transform, "Scale: ", scale_get, scale_set, transform->SCALE_CHANGED_EVENT, PropDesc().Tag(Tags::VECTOR2).Step(1));
    
    
}

void InspectorVisitor::Visit(SpriteRenderer* renderer){

    auto color_get = [=](){
        return renderer->GetColor();
    };
    auto color_set = [=](glm::vec4 color){
        renderer->SetColor(color);
    };
    auto flipX_get = [=](){
        return renderer->GetFlipX();
    };
    auto flipX_set = [=](bool val){
        renderer->SetFlipX(val);
    };
    auto flipY_get = [=](){
        return renderer->GetFlipY();
    };
    auto flipY_set = [=](bool val){
        renderer->SetFlipY(val);
    };
    
    auto visible_get = [=](){
        return renderer->GetVisible();
    };

    auto visible_set = [=](bool val){
        renderer->SetVisible(val);
    };

    auto material_get = [=](){
        auto* mat = renderer->GetMaterial();
        return mat? mat->GetID() : "";
    };

    auto material_set = [=](std::string val){
        renderer->SetMaterial(val);
    };
    auto sprite_get = [=](){
        auto* sprite = renderer->GetSprite();
        return sprite? sprite->GetID() : "";
    };

    auto sprite_set = [=](std::string val){
        renderer->SetSprite(val);
    };



    BindProperty<glm::vec4>(renderer, "Color: ", color_get, color_set, renderer->COLOR_CHANGED_EVENT, PropDesc().Tag(Tags::COLOR));
    BindProperty<bool>(renderer, "Flip X: ", flipX_get, flipX_set, renderer->FLIP_X_CHANGED_EVENT, PropDesc().Tag(Tags::TOGGLE));
    BindProperty<bool>(renderer, "Flip Y: ", flipY_get, flipY_set, renderer->FLIP_Y_CHANGED_EVENT, PropDesc().Tag(Tags::TOGGLE));
    BindProperty<bool>(renderer, "Visible: ", visible_get, visible_set, renderer->VISIBILITY_CHANGED_EVENT, PropDesc().Tag(Tags::TOGGLE));
    BindProperty<std::string>(renderer, "Material: ", material_get, material_set, renderer->MATERIAL_CHANGED_EVENT, PropDesc().Tag(Tags::MATERIAL));
    BindProperty<std::string>(renderer, "Sprite: ", sprite_get, sprite_set, renderer->MATERIAL_CHANGED_EVENT, PropDesc().Tag(Tags::SPRITE));

}

void InspectorVisitor::AddRow(const std::string& text, QWidget* widget, bool stretch){
    QHBoxLayout* hbox = new QHBoxLayout();

    QLabel* label = new QLabel(text.c_str());
    auto font = label->font();
    font.setBold(true);
    label->setFont(font);
    label->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    hbox->addWidget(label);
    if (stretch) hbox->addStretch();
    hbox->addWidget(widget);
    layout->addLayout(hbox);
    containsContent = true;
}